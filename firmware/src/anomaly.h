#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifndef SPIKE_PROB_PCT
  #define SPIKE_PROB_PCT 2
#endif

static constexpr float BONUS_NOISE_SIGMA     = 0.2f;
static constexpr float SPIKE_AMPLITUDE_MIN   = 5.0f;
static constexpr float SPIKE_AMPLITUDE_MAX   = 15.0f;
static constexpr uint8_t ANOMALY_SPIKE_PROB_PCT = SPIKE_PROB_PCT;

// ── Anomaly detection — Z-score and Hampel identifier ─────────────────────────
//
// Usage (called once per sample from sampler_task):
//   bool spike;
//   float reference = raw_sample + gaussian_noise(BONUS_NOISE_SIGMA);
//   float s = inject_spike(reference, &spike);
//   anomaly_process(reference, s, spike, window_mean, window_std);
//
// Print evaluation stats every 5 s:
//   anomaly_print_stats(fs_hz);
//   anomaly_print_window_analysis(fs_hz);

// ── Spike injection ───────────────────────────────────────────────────────────

// Replaces `sample` with a signed uniform outlier in ±U(5, 15)
// with probability SPIKE_PROB.
// Sets *is_spike to true when an injection occurred.
float inject_spike(float sample, bool* is_spike);

// Additive Gaussian noise via Box-Muller; mean=0, given sigma.
float gaussian_noise(float sigma);

// ── Detectors ────────────────────────────────────────────────────────────────

// Z-score: |sample − mean| > 3σ
// Uses the provided mean/std (computed from the ring buffer).
bool zscore_detect(float sample, float mean, float std_dev);

// Hampel identifier with half-window k (full window = 2k+1).
// Internally maintains a sliding window + ground-truth mirror.
// Returns true when the *center* sample (k steps old) is an anomaly.
// Sets *center_out  = the center sample value being evaluated.
// Sets *gt_out      = was the center sample a spike (ground truth)?
// Sets *reference_out = uncontaminated reference for that center sample.
// Sets *median_out    = median of the full Hampel window.
// Returns false (and leaves outputs unchanged) when the window is not yet full.
bool hampel_detect(uint8_t k_idx,   // 0 = k5, 1 = k20, 2 = k50
                   float sample, bool is_spike, float reference,
                   float* center_out, bool* gt_out,
                   float* reference_out, float* median_out);

// ── Stats tracking ────────────────────────────────────────────────────────────

struct AnomalyStats {
    uint32_t tp, fp, tn, fn;
};

// Record one evaluation outcome.
void stats_record(AnomalyStats* s, bool detected, bool ground_truth);

float stats_tpr(const AnomalyStats* s);  // TP / (TP + FN)
float stats_fpr(const AnomalyStats* s);  // FP / (FP + TN)

// ── High-level entry points ───────────────────────────────────────────────────

void anomaly_init();
const char* anomaly_signal_family();
void anomaly_print_config();

// Call once per sample from sampler_task.
// `reference` is the uncontaminated sample after Gaussian noise but before spike injection.
// mean and std are the current ring-buffer statistics.
void anomaly_process(float reference, float sample, bool is_spike, float mean, float std_dev);

// Call from aggregator_task every 5 s.
void anomaly_print_stats(float current_fs_hz);
void anomaly_print_window_analysis(float current_fs_hz);
