#include "anomaly.h"
#include "sensor.h"
#include "tasks.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <esp_timer.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static constexpr float SPIKE_PROB = (float)SPIKE_PROB_PCT / 100.0f;
static const uint8_t HAMPEL_K[3] = {5, 20, 50};
static const uint8_t HAMPEL_SIZE[3] = {11, 41, 101};

struct HampelState {
    float win[101];
    float ref[101];
    bool spike[101];
    uint8_t k;
    uint8_t sz;
    uint8_t count;
};

struct FilterMetrics {
    AnomalyStats stats;
    uint64_t exec_us = 0;
    uint32_t exec_count = 0;
    double raw_abs_err_sum = 0.0;
    double filtered_abs_err_sum = 0.0;
    uint32_t error_count = 0;
    uint32_t ram_bytes = 0;
};

static HampelState g_hampel[3];
static FilterMetrics g_zscore_metrics;
static FilterMetrics g_hampel_metrics[3];
static uint32_t s_last_bonus_fft_seq = 0;

static int cmp_float(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

static float median_of(const float* arr, uint8_t n) {
    float tmp[101];
    memcpy(tmp, arr, n * sizeof(float));
    qsort(tmp, n, sizeof(float), cmp_float);
    return tmp[n / 2];
}

static float median_abs_deviation(const float* arr, uint8_t n, float med) {
    float devs[101];
    for (uint8_t i = 0; i < n; i++) {
        devs[i] = fabsf(arr[i] - med);
    }
    return median_of(devs, n);
}

static float clamp_adaptive_fs(float dominant_hz) {
    float fs = 2.0f * dominant_hz;
    if (fs < 10.0f) fs = 10.0f;
    if (fs > 100.0f) fs = 100.0f;
    return fs;
}

static float mer_from_metrics(const FilterMetrics& m) {
    if (m.error_count == 0 || m.raw_abs_err_sum <= 1e-9) return 0.0f;
    return 1.0f - (float)(m.filtered_abs_err_sum / m.raw_abs_err_sum);
}

static float major_peak_from_samples(const double* values, uint16_t count, float fs_hz) {
    static double real[TASKS_FFT_N];
    static double imag[TASKS_FFT_N];

    if (count != TASKS_FFT_N || fs_hz <= 0.0f) return 0.0f;

    memcpy(real, values, sizeof(real));
    memset(imag, 0, sizeof(imag));

    ArduinoFFT<double> fft(real, imag, count, (double)fs_hz);
    fft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.compute(FFT_FORWARD);
    fft.complexToMagnitude();
    return (float)fft.majorPeak();
}

static void apply_zscore_filter_window(const double* raw, uint16_t count, double* out) {
    double sum = 0.0;
    for (uint16_t i = 0; i < count; i++) sum += raw[i];
    double mean = (count > 0) ? (sum / (double)count) : 0.0;

    double var = 0.0;
    for (uint16_t i = 0; i < count; i++) {
        double d = raw[i] - mean;
        var += d * d;
    }
    double std_dev = (count > 0) ? sqrt(var / (double)count) : 0.0;
    double threshold = 3.0 * std_dev;

    for (uint16_t i = 0; i < count; i++) {
        bool detected = (std_dev >= 0.001) && (fabs(raw[i] - mean) > threshold);
        out[i] = detected ? mean : raw[i];
    }
}

static void apply_hampel_filter_window(const double* raw, uint16_t count, uint8_t k, double* out) {
    for (uint16_t i = 0; i < count; i++) out[i] = raw[i];
    if (count < (uint16_t)(2 * k + 1)) return;

    float win[101];
    for (uint16_t center = k; center + k < count; center++) {
        uint8_t sz = (uint8_t)(2 * k + 1);
        for (uint8_t j = 0; j < sz; j++) {
            win[j] = (float)raw[center - k + j];
        }
        float med = median_of(win, sz);
        float mad = median_abs_deviation(win, sz, med);
        float threshold = 3.0f * 1.4826f * mad;
        if (threshold < 0.001f) continue;
        if (fabsf((float)raw[center] - med) > threshold) {
            out[center] = (double)med;
        }
    }
}

const char* anomaly_signal_family() {
#if defined(SIGNAL_MODE) && SIGNAL_MODE == 0
    return "clean";
#elif defined(SIGNAL_MODE) && SIGNAL_MODE == 1
    return "noise";
#else
    return "spikes";
#endif
}

void anomaly_print_config() {
    Serial.printf(
        "[BONUS-CONFIG] mode=%s noise_sigma=%.2f spike_pct=%u spike_dist=signed_uniform_%.1f_%.1f baseline_fs=100 fft_n=%u variant=%s expected_fmax=%.1f formula=%s\n",
        anomaly_signal_family(),
        BONUS_NOISE_SIGMA,
        (unsigned int)((strcmp(anomaly_signal_family(), "spikes") == 0) ? ANOMALY_SPIKE_PROB_PCT : 0),
        SPIKE_AMPLITUDE_MIN,
        SPIKE_AMPLITUDE_MAX,
        (unsigned int)TASKS_FFT_N,
        clean_signal_variant_label(),
        clean_signal_variant_expected_fmax_hz(),
        clean_signal_variant_formula()
    );
}

float inject_spike(float sample, bool* is_spike) {
    *is_spike = false;
    float r = (float)esp_random() / (float)UINT32_MAX;
    if (r >= SPIKE_PROB) return sample;

    float mag_unit = (float)esp_random() / (float)UINT32_MAX;
    float amplitude = SPIKE_AMPLITUDE_MIN + mag_unit * (SPIKE_AMPLITUDE_MAX - SPIKE_AMPLITUDE_MIN);
    *is_spike = true;
    return ((esp_random() & 1) ? amplitude : -amplitude);
}

float gaussian_noise(float sigma) {
    float u1 = (float)esp_random() / (float)UINT32_MAX;
    float u2 = (float)esp_random() / (float)UINT32_MAX;
    if (u1 < 1e-6f) u1 = 1e-6f;
    return sigma * sqrtf(-2.0f * logf(u1)) * cosf(TWO_PI * u2);
}

bool zscore_detect(float sample, float mean, float std_dev) {
    if (std_dev < 0.001f) return false;
    return fabsf(sample - mean) > 3.0f * std_dev;
}

bool hampel_detect(uint8_t k_idx,
                   float sample, bool is_spike, float reference,
                   float* center_out, bool* gt_out,
                   float* reference_out, float* median_out) {
    HampelState* h = &g_hampel[k_idx];

    if (h->count < h->sz) {
        h->win[h->count] = sample;
        h->ref[h->count] = reference;
        h->spike[h->count] = is_spike;
        h->count++;
        return false;
    }

    memmove(h->win, h->win + 1, (h->sz - 1) * sizeof(float));
    memmove(h->ref, h->ref + 1, (h->sz - 1) * sizeof(float));
    memmove(h->spike, h->spike + 1, (h->sz - 1) * sizeof(bool));
    h->win[h->sz - 1] = sample;
    h->ref[h->sz - 1] = reference;
    h->spike[h->sz - 1] = is_spike;

    float center = h->win[h->k];
    float med = median_of(h->win, h->sz);
    float mad = median_abs_deviation(h->win, h->sz, med);
    float threshold = 3.0f * 1.4826f * mad;

    *center_out = center;
    *gt_out = h->spike[h->k];
    *reference_out = h->ref[h->k];
    *median_out = med;

    if (threshold < 0.001f) return false;
    return fabsf(center - med) > threshold;
}

void stats_record(AnomalyStats* s, bool detected, bool ground_truth) {
    if (ground_truth && detected) s->tp++;
    else if (ground_truth && !detected) s->fn++;
    else if (!ground_truth && detected) s->fp++;
    else s->tn++;
}

float stats_tpr(const AnomalyStats* s) {
    uint32_t pos = s->tp + s->fn;
    return pos ? (float)s->tp / (float)pos : 0.0f;
}

float stats_fpr(const AnomalyStats* s) {
    uint32_t neg = s->fp + s->tn;
    return neg ? (float)s->fp / (float)neg : 0.0f;
}

void anomaly_init() {
    memset(&g_zscore_metrics, 0, sizeof(g_zscore_metrics));
    memset(g_hampel_metrics, 0, sizeof(g_hampel_metrics));
    s_last_bonus_fft_seq = 0;

    for (uint8_t i = 0; i < 3; i++) {
        memset(&g_hampel[i], 0, sizeof(HampelState));
        g_hampel[i].k = HAMPEL_K[i];
        g_hampel[i].sz = HAMPEL_SIZE[i];
        g_hampel[i].count = 0;
        g_hampel_metrics[i].ram_bytes =
            (uint32_t)(HAMPEL_SIZE[i] * (sizeof(float) * 2 + sizeof(bool)));
    }
}

void anomaly_process(float reference, float sample, bool is_spike, float mean, float std_dev) {
    int64_t t0 = esp_timer_get_time();
    bool z_detected = zscore_detect(sample, mean, std_dev);
    float z_repaired = z_detected ? mean : sample;
    g_zscore_metrics.exec_us += (uint64_t)(esp_timer_get_time() - t0);
    g_zscore_metrics.exec_count++;
    g_zscore_metrics.raw_abs_err_sum += fabs((double)sample - (double)reference);
    g_zscore_metrics.filtered_abs_err_sum += fabs((double)z_repaired - (double)reference);
    g_zscore_metrics.error_count++;
    stats_record(&g_zscore_metrics.stats, z_detected, is_spike);

    for (uint8_t i = 0; i < 3; i++) {
        bool window_was_full = (g_hampel[i].count == g_hampel[i].sz);
        float center = 0.0f;
        bool gt = false;
        float center_ref = 0.0f;
        float median = 0.0f;

        t0 = esp_timer_get_time();
        bool h_detected = hampel_detect(i, sample, is_spike, reference,
                                        &center, &gt, &center_ref, &median);
        g_hampel_metrics[i].exec_us += (uint64_t)(esp_timer_get_time() - t0);
        g_hampel_metrics[i].exec_count++;

        if (window_was_full) {
            float repaired = h_detected ? median : center;
            g_hampel_metrics[i].raw_abs_err_sum += fabs((double)center - (double)center_ref);
            g_hampel_metrics[i].filtered_abs_err_sum += fabs((double)repaired - (double)center_ref);
            g_hampel_metrics[i].error_count++;
            stats_record(&g_hampel_metrics[i].stats, h_detected, gt);
        }
    }
}

void anomaly_print_stats(float current_fs_hz) {
    Serial.printf("[ANOMALY] Z-score:   TPR=%.3f  FPR=%.3f  (TP=%lu FP=%lu FN=%lu TN=%lu)\n",
                  stats_tpr(&g_zscore_metrics.stats), stats_fpr(&g_zscore_metrics.stats),
                  (unsigned long)g_zscore_metrics.stats.tp, (unsigned long)g_zscore_metrics.stats.fp,
                  (unsigned long)g_zscore_metrics.stats.fn, (unsigned long)g_zscore_metrics.stats.tn);

    const char* labels[3] = {"k=5 ", "k=20", "k=50"};
    for (uint8_t i = 0; i < 3; i++) {
        Serial.printf("[ANOMALY] Hampel %s: TPR=%.3f  FPR=%.3f  (TP=%lu FP=%lu FN=%lu TN=%lu)\n",
                      labels[i],
                      stats_tpr(&g_hampel_metrics[i].stats), stats_fpr(&g_hampel_metrics[i].stats),
                      (unsigned long)g_hampel_metrics[i].stats.tp, (unsigned long)g_hampel_metrics[i].stats.fp,
                      (unsigned long)g_hampel_metrics[i].stats.fn, (unsigned long)g_hampel_metrics[i].stats.tn);
    }

    Serial.printf("[ANOMALY] exec avg — Z-score: %.2f µs  Hampel k=5: %.2f µs  k=20: %.2f µs  k=50: %.2f µs\n",
                  g_zscore_metrics.exec_count ? (float)g_zscore_metrics.exec_us / g_zscore_metrics.exec_count : 0.0f,
                  g_hampel_metrics[0].exec_count ? (float)g_hampel_metrics[0].exec_us / g_hampel_metrics[0].exec_count : 0.0f,
                  g_hampel_metrics[1].exec_count ? (float)g_hampel_metrics[1].exec_us / g_hampel_metrics[1].exec_count : 0.0f,
                  g_hampel_metrics[2].exec_count ? (float)g_hampel_metrics[2].exec_us / g_hampel_metrics[2].exec_count : 0.0f);

    Serial.printf("[ANOMALY] Hampel RAM  k=5: %u B  k=20: %u B  k=50: %u B  (win[]+ref[]+spike[] per instance)\n",
                  g_hampel_metrics[0].ram_bytes,
                  g_hampel_metrics[1].ram_bytes,
                  g_hampel_metrics[2].ram_bytes);

    if (strcmp(anomaly_signal_family(), "spikes") != 0) return;

    Serial.printf("[BONUS-FILTER] filter=zscore k=0 fs=%.1f tpr=%.3f fpr=%.3f mer=%.3f exec_us=%.2f ram_b=0 added_delay_ms=0.0\n",
                  current_fs_hz,
                  stats_tpr(&g_zscore_metrics.stats), stats_fpr(&g_zscore_metrics.stats),
                  mer_from_metrics(g_zscore_metrics),
                  g_zscore_metrics.exec_count ? (float)g_zscore_metrics.exec_us / g_zscore_metrics.exec_count : 0.0f);

    for (uint8_t i = 0; i < 3; i++) {
        float added_delay_ms = (current_fs_hz > 0.0f) ? (1000.0f * (float)HAMPEL_K[i] / current_fs_hz) : 0.0f;
        Serial.printf("[BONUS-FILTER] filter=hampel k=%u fs=%.1f tpr=%.3f fpr=%.3f mer=%.3f exec_us=%.2f ram_b=%u added_delay_ms=%.1f\n",
                      (unsigned int)HAMPEL_K[i], current_fs_hz,
                      stats_tpr(&g_hampel_metrics[i].stats), stats_fpr(&g_hampel_metrics[i].stats),
                      mer_from_metrics(g_hampel_metrics[i]),
                      g_hampel_metrics[i].exec_count ? (float)g_hampel_metrics[i].exec_us / g_hampel_metrics[i].exec_count : 0.0f,
                      g_hampel_metrics[i].ram_bytes,
                      added_delay_ms);
    }
}

void anomaly_print_window_analysis(float current_fs_hz) {
    if (strcmp(anomaly_signal_family(), "spikes") != 0) return;

    double raw[TASKS_FFT_N];
    double reference[TASKS_FFT_N];
    uint16_t raw_count = 0;
    uint16_t ref_count = 0;
    float raw_fs = 0.0f;
    float ref_fs = 0.0f;
    uint32_t raw_seq = 0;
    uint32_t ref_seq = 0;

    if (!copy_last_fft_window(raw, TASKS_FFT_N, &raw_count, &raw_fs, &raw_seq)) return;
    if (!copy_last_fft_reference_window(reference, TASKS_FFT_N, &ref_count, &ref_fs, &ref_seq)) return;
    if (raw_count != TASKS_FFT_N || ref_count != TASKS_FFT_N) return;
    if (raw_seq != ref_seq || raw_fs <= 0.0f || ref_fs <= 0.0f) return;
    if (raw_seq == s_last_bonus_fft_seq) return;
    s_last_bonus_fft_seq = raw_seq;

    double zscore_filtered[TASKS_FFT_N];
    double hampel5_filtered[TASKS_FFT_N];
    double hampel20_filtered[TASKS_FFT_N];
    double hampel50_filtered[TASKS_FFT_N];

    apply_zscore_filter_window(raw, raw_count, zscore_filtered);
    apply_hampel_filter_window(raw, raw_count, 5, hampel5_filtered);
    apply_hampel_filter_window(raw, raw_count, 20, hampel20_filtered);
    apply_hampel_filter_window(raw, raw_count, 50, hampel50_filtered);

    float raw_peak = major_peak_from_samples(raw, raw_count, raw_fs);
    float zscore_peak = major_peak_from_samples(zscore_filtered, raw_count, raw_fs);
    float hampel5_peak = major_peak_from_samples(hampel5_filtered, raw_count, raw_fs);
    float hampel20_peak = major_peak_from_samples(hampel20_filtered, raw_count, raw_fs);
    float hampel50_peak = major_peak_from_samples(hampel50_filtered, raw_count, raw_fs);

    Serial.printf("[BONUS-FFT] seq=%lu raw_peak=%.2f zscore_peak=%.2f hampel5_peak=%.2f hampel20_peak=%.2f hampel50_peak=%.2f raw_fs=%.1f zscore_fs=%.1f hampel5_fs=%.1f hampel20_fs=%.1f hampel50_fs=%.1f\n",
                  (unsigned long)raw_seq,
                  raw_peak, zscore_peak, hampel5_peak, hampel20_peak, hampel50_peak,
                  clamp_adaptive_fs(raw_peak), clamp_adaptive_fs(zscore_peak),
                  clamp_adaptive_fs(hampel5_peak), clamp_adaptive_fs(hampel20_peak),
                  clamp_adaptive_fs(hampel50_peak));

    Serial.printf("[FFT-CONTAM] raw_peak=%.2f Hz  filtered_peak=%.2f Hz  (expected 5.00 Hz)\n",
                  raw_peak, zscore_peak);
}
