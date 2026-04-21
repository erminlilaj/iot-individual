#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── Anomaly detection — Z-score and Hampel identifier ─────────────────────────
//
// Usage (called once per sample from sampler_task):
//   bool spike;
//   float s = inject_spike(raw_sample, &spike);
//   anomaly_process(s, spike, window_mean, window_std);
//
// Print evaluation stats every 5 s:
//   anomaly_print_stats();

// ── Spike injection ───────────────────────────────────────────────────────────

// Replaces `sample` with ±20.0 with probability SPIKE_PROB.
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
// Returns false (and leaves outputs unchanged) when the window is not yet full.
bool hampel_detect(uint8_t k_idx,   // 0 = k5, 1 = k20, 2 = k50
                   float sample, bool is_spike,
                   float* center_out, bool* gt_out);

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

// Call once per sample from sampler_task.
// mean and std are the current ring-buffer statistics.
void anomaly_process(float sample, bool is_spike, float mean, float std_dev);

// Call from aggregator_task every 5 s.
void anomaly_print_stats();
