#include "anomaly.h"
#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <esp_timer.h>

// ── Spike injection ───────────────────────────────────────────────────────────

#ifndef SPIKE_PROB_PCT
  #define SPIKE_PROB_PCT 2
#endif
static constexpr float SPIKE_PROB      = (float)SPIKE_PROB_PCT / 100.0f;
static constexpr float SPIKE_AMPLITUDE = 20.0f;

float inject_spike(float sample, bool* is_spike) {
    *is_spike = false;
    // esp_random() gives a uniform uint32 in [0, UINT32_MAX]
    float r = (float)esp_random() / (float)UINT32_MAX;
    if (r < SPIKE_PROB) {
        *is_spike = true;
        return ((esp_random() & 1) ? SPIKE_AMPLITUDE : -SPIKE_AMPLITUDE);
    }
    return sample;
}

// ── Gaussian noise (Box-Muller) ───────────────────────────────────────────────

float gaussian_noise(float sigma) {
    float u1 = (float)esp_random() / (float)UINT32_MAX;
    float u2 = (float)esp_random() / (float)UINT32_MAX;
    if (u1 < 1e-6f) u1 = 1e-6f;
    return sigma * sqrtf(-2.0f * logf(u1)) * cosf(TWO_PI * u2);
}

// ── Z-score ───────────────────────────────────────────────────────────────────

bool zscore_detect(float sample, float mean, float std_dev) {
    if (std_dev < 0.001f) return false;
    return fabsf(sample - mean) > 3.0f * std_dev;
}

// ── Hampel identifier ─────────────────────────────────────────────────────────
//
// Three instances: k = 5, 20, 50.  Full window sizes: 11, 41, 101.
// Sliding window is stored oldest-to-newest in win[0..win_size-1].
// win[k] is always the center sample being evaluated.
// When a new sample arrives we memmove left and write at win[win_size-1].

static const uint8_t  HAMPEL_K[3]      = { 5, 20, 50 };
static const uint8_t  HAMPEL_SIZE[3]   = { 11, 41, 101 };

struct HampelState {
    float   win[101];
    bool    spike[101]; // ground-truth mirror
    uint8_t k;
    uint8_t sz;
    uint8_t count; // saturates at sz
};

static HampelState g_hampel[3];

static int cmp_float(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float median_of(float* arr, uint8_t n) {
    float tmp[101];
    memcpy(tmp, arr, n * sizeof(float));
    qsort(tmp, n, sizeof(float), cmp_float);
    return tmp[n / 2];
}

bool hampel_detect(uint8_t k_idx,
                   float sample, bool is_spike,
                   float* center_out, bool* gt_out) {
    HampelState* h = &g_hampel[k_idx];

    if (h->count < h->sz) {
        h->win[h->count]   = sample;
        h->spike[h->count] = is_spike;
        h->count++;
        return false;
    }

    // Shift window left by one, append new sample at end
    memmove(h->win,   h->win   + 1, (h->sz - 1) * sizeof(float));
    memmove(h->spike, h->spike + 1, (h->sz - 1) * sizeof(bool));
    h->win[h->sz - 1]   = sample;
    h->spike[h->sz - 1] = is_spike;

    // Center sample = win[k]
    float center = h->win[h->k];
    *center_out  = center;
    *gt_out      = h->spike[h->k];

    // Compute median of full window
    float med = median_of(h->win, h->sz);

    // Compute MAD: median of |win[i] - median|
    float devs[101];
    for (uint8_t i = 0; i < h->sz; i++) {
        devs[i] = fabsf(h->win[i] - med);
    }
    float mad = median_of(devs, h->sz);

    // Hampel threshold: 3 × 1.4826 × MAD
    float threshold = 3.0f * 1.4826f * mad;

    // Degenerate case: all samples identical (signal is zero etc.)
    if (threshold < 0.001f) return false;

    return fabsf(center - med) > threshold;
}

// ── Stats helpers ─────────────────────────────────────────────────────────────

void stats_record(AnomalyStats* s, bool detected, bool ground_truth) {
    if (ground_truth  &&  detected) s->tp++;
    else if (ground_truth  && !detected) s->fn++;
    else if (!ground_truth &&  detected) s->fp++;
    else                                  s->tn++;
}

float stats_tpr(const AnomalyStats* s) {
    uint32_t pos = s->tp + s->fn;
    return pos ? (float)s->tp / (float)pos : 0.0f;
}

float stats_fpr(const AnomalyStats* s) {
    uint32_t neg = s->fp + s->tn;
    return neg ? (float)s->fp / (float)neg : 0.0f;
}

// ── Global stats ──────────────────────────────────────────────────────────────

static AnomalyStats g_zscore_stats;
static AnomalyStats g_hampel_stats[3];

// Execution time accumulators (µs)
static uint64_t s_zscore_us       = 0;
static uint32_t s_zscore_count    = 0;
static uint64_t s_hampel_us[3]    = {0, 0, 0};
static uint32_t s_hampel_count[3] = {0, 0, 0};

void anomaly_init() {
    memset(&g_zscore_stats, 0, sizeof(g_zscore_stats));
    memset(g_hampel_stats,  0, sizeof(g_hampel_stats));

    for (uint8_t i = 0; i < 3; i++) {
        memset(&g_hampel[i], 0, sizeof(HampelState));
        g_hampel[i].k     = HAMPEL_K[i];
        g_hampel[i].sz    = HAMPEL_SIZE[i];
        g_hampel[i].count = 0;
    }
}

void anomaly_process(float sample, bool is_spike, float mean, float std_dev) {
    // Z-score (instant evaluation) — time it
    int64_t t0 = esp_timer_get_time();
    bool z_detected = zscore_detect(sample, mean, std_dev);
    s_zscore_us += (uint64_t)(esp_timer_get_time() - t0);
    s_zscore_count++;
    stats_record(&g_zscore_stats, z_detected, is_spike);

    // Hampel instances (k-sample delayed evaluation).
    // Check window-full BEFORE the call: hampel_detect only sets center/gt
    // when the window was already full (memmove path).
    for (uint8_t i = 0; i < 3; i++) {
        bool window_was_full = (g_hampel[i].count == g_hampel[i].sz);
        float center = 0.0f; bool gt = false;
        t0 = esp_timer_get_time();
        bool h_detected = hampel_detect(i, sample, is_spike, &center, &gt);
        s_hampel_us[i]    += (uint64_t)(esp_timer_get_time() - t0);
        s_hampel_count[i]++;
        if (window_was_full) {
            stats_record(&g_hampel_stats[i], h_detected, gt);
        }
    }
}

void anomaly_print_stats() {
    Serial.printf("[ANOMALY] Z-score:   TPR=%.3f  FPR=%.3f  (TP=%lu FP=%lu FN=%lu TN=%lu)\n",
        stats_tpr(&g_zscore_stats), stats_fpr(&g_zscore_stats),
        (unsigned long)g_zscore_stats.tp, (unsigned long)g_zscore_stats.fp,
        (unsigned long)g_zscore_stats.fn, (unsigned long)g_zscore_stats.tn);

    const char* labels[3] = {"k=5 ", "k=20", "k=50"};
    for (uint8_t i = 0; i < 3; i++) {
        Serial.printf("[ANOMALY] Hampel %s: TPR=%.3f  FPR=%.3f  (TP=%lu FP=%lu FN=%lu TN=%lu)\n",
            labels[i],
            stats_tpr(&g_hampel_stats[i]), stats_fpr(&g_hampel_stats[i]),
            (unsigned long)g_hampel_stats[i].tp, (unsigned long)g_hampel_stats[i].fp,
            (unsigned long)g_hampel_stats[i].fn, (unsigned long)g_hampel_stats[i].tn);
    }

    // Execution time averages
    Serial.printf("[ANOMALY] exec avg — Z-score: %.2f µs  Hampel k=5: %.2f µs  k=20: %.2f µs  k=50: %.2f µs\n",
        s_zscore_count    ? (float)s_zscore_us    / s_zscore_count    : 0.0f,
        s_hampel_count[0] ? (float)s_hampel_us[0] / s_hampel_count[0] : 0.0f,
        s_hampel_count[1] ? (float)s_hampel_us[1] / s_hampel_count[1] : 0.0f,
        s_hampel_count[2] ? (float)s_hampel_us[2] / s_hampel_count[2] : 0.0f);

    // Memory footprint of each Hampel window
    Serial.printf("[ANOMALY] Hampel RAM  k=5: %u B  k=20: %u B  k=50: %u B  (win[]+spike[] per instance)\n",
        (uint32_t)(HAMPEL_SIZE[0] * (sizeof(float) + sizeof(bool))),
        (uint32_t)(HAMPEL_SIZE[1] * (sizeof(float) + sizeof(bool))),
        (uint32_t)(HAMPEL_SIZE[2] * (sizeof(float) + sizeof(bool))));
}
