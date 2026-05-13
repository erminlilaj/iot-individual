#include "fft_analysis.h"
#include "sensor.h"
#include "anomaly.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <math.h>

static const uint16_t N = 256;   // FFT window size — must be a power of 2

float compute_optimal_fs(float fs_current) {
    static double real[N];
    static double imag[N];

    float t = 0.0f;
    float dt = 1.0f / fs_current;

    // Collect N samples at the current sampling rate
    for (uint16_t i = 0; i < N; i++) {
        real[i] = (double)generate_sample(t);
        imag[i] = 0.0;          // imaginary part is zero for a real-valued signal
        t += dt;

        // Honour the sampling interval so the FFT sees the correct time spacing
        delayMicroseconds((uint32_t)(dt * 1e6f));
    }

    ArduinoFFT<double> fft(real, imag, N, (double)fs_current);

    // Hamming window reduces spectral leakage at the edges of the recording
    fft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);

    fft.compute(FFT_FORWARD);

    // Convert complex result to magnitude spectrum (modifies real[] in-place)
    fft.complexToMagnitude();

    // Find the frequency of the bin with the highest magnitude
    double dominant_freq = fft.majorPeak();

    // Nyquist: minimum valid sampling rate = 2 × highest frequency present
    float optimal_fs = 2.0f * (float)dominant_freq;

    Serial.println("──────────────────────────────");
    Serial.println("Phase 3 — FFT Analysis");
    Serial.print("  Sampling rate used      : ");
    Serial.print(fs_current, 1);
    Serial.println(" Hz");
    Serial.print("  Dominant frequency      : ");
    Serial.print(dominant_freq, 2);
    Serial.println(" Hz  (expected: 5.00 Hz)");
    Serial.print("  Optimal fs (2×f_dom)    : ");
    Serial.print(optimal_fs, 1);
    Serial.println(" Hz  (Nyquist minimum: 10.0 Hz; app target: 40.0 Hz)");
    Serial.println("──────────────────────────────");

    return optimal_fs;
}

void compute_fft_contamination_report(float /*fs*/, float* out_raw, float* out_filtered) {
    static double raw_r[N], raw_i[N];
    static double flt_r[N], flt_i[N];
    static float samples[N];
    // Use a fixed 100 Hz rate so both the 3 Hz and 5 Hz components sit well
    // within the spectrum (far from Nyquist). At the old adaptive 10 Hz rate the
    // 5 Hz component fell exactly on Nyquist and was hard for majorPeak().
    const float fs = 100.0f;
    float t = 0.0f, dt = 1.0f / fs;

    // Collect N samples with spike injection; accumulate mean for Z-score pass
    float sum = 0.0f;
    for (uint16_t i = 0; i < N; i++) {
        bool spike;
        samples[i] = inject_spike(generate_sample(t), &spike);
        sum += samples[i];
        t   += dt;
    }
    float mean = sum / (float)N;

    // Compute std dev for Z-score threshold
    float var = 0.0f;
    for (uint16_t i = 0; i < N; i++) {
        float d = samples[i] - mean;
        var += d * d;
    }
    float std_dev = sqrtf(var / (float)N);

    // Fill raw and filtered buffers
    for (uint16_t i = 0; i < N; i++) {
        raw_r[i] = (double)samples[i];
        raw_i[i] = 0.0;
        // Z-score filter: replace spike with mean
        float s_flt = (std_dev > 0.001f && fabsf(samples[i] - mean) > 3.0f * std_dev)
                      ? mean : samples[i];
        flt_r[i] = (double)s_flt;
        flt_i[i] = 0.0;
    }

    ArduinoFFT<double> fft_raw(raw_r, raw_i, N, (double)fs);
    fft_raw.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft_raw.compute(FFT_FORWARD);
    fft_raw.complexToMagnitude();
    *out_raw = (float)fft_raw.majorPeak();

    ArduinoFFT<double> fft_flt(flt_r, flt_i, N, (double)fs);
    fft_flt.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft_flt.compute(FFT_FORWARD);
    fft_flt.complexToMagnitude();
    *out_filtered = (float)fft_flt.majorPeak();
}
