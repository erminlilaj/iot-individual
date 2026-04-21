#pragma once

// Returns the recommended sampling rate in Hz: 2 × dominant_frequency.
// Collects N=256 samples at fs_current Hz, runs FFT, finds the dominant peak.
float compute_optimal_fs(float fs_current);

// Bonus: measure how spikes distort the FFT dominant frequency.
// Collects 256 samples with spike injection, runs FFT on the raw contaminated
// signal and on a Z-score-cleaned copy, returns both dominant frequency estimates.
// Only meaningful when SIGNAL_MODE == 2 (spike injection active).
void compute_fft_contamination_report(float fs, float* out_raw, float* out_filtered);
