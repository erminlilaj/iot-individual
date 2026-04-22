#include "tasks.h"
#include "sensor.h"
#include "aggregator.h"
#include "energy_model.h"
#include "anomaly.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <freertos/queue.h>
#include <esp_timer.h>
#include <string.h>

// ── Shared state ─────────────────────────────────────────────────────────────

volatile float    g_fs_current      = 100.0f;
volatile float    g_last_fft_dominant_hz = 0.0f;
SemaphoreHandle_t g_fs_mutex;
volatile int64_t  g_window_start_us = 0;

// ── Double buffer (ping-pong) ─────────────────────────────────────────────────
//
// The sampler fills one half while the FFT task reads the other.
// Each buffer holds FFT_N doubles for the real part; imaginary starts at 0.

static const uint16_t FFT_N = 256;

static double buf_real[2][FFT_N];
static double buf_imag[2][FFT_N];

static double          g_last_fft_samples[FFT_N];
static uint16_t        g_last_fft_count = 0;
static float           g_last_fft_fs    = 0.0f;
static uint32_t        g_last_fft_seq   = 0;
static SemaphoreHandle_t g_fft_capture_mutex;

// ── Job descriptor sent through the queue ────────────────────────────────────

struct FFTJob {
    uint8_t buf_idx;   // which buffer (0 or 1) is ready for FFT
    float   fs;        // sampling rate that was used to fill it
};

static QueueHandle_t g_fft_queue;   // depth 1 — one pending job at a time

#if defined(PLOT_CAPTURE)
static void emit_plot_capture(uint32_t seq) {
    double snapshot[FFT_N];
    uint16_t sample_count = 0;
    float snapshot_fs = 0.0f;
    uint32_t snapshot_seq = 0;
    if (!copy_last_fft_window(snapshot, FFT_N, &sample_count, &snapshot_fs, &snapshot_seq)) {
        return;
    }
    if (snapshot_seq != seq || sample_count == 0) {
        return;
    }

    Serial.printf("[PLOT] type=fft_window seq=%lu fs=%.3f n=%u\n",
                  (unsigned long)seq, snapshot_fs, sample_count);
    Serial.print("[PLOT-SAMPLES] seq=");
    Serial.print((unsigned long)seq);
    Serial.print(" values=");
    for (uint16_t i = 0; i < sample_count; i++) {
        Serial.printf("%.6f", snapshot[i]);
        if (i + 1 < sample_count) Serial.print(",");
    }
    Serial.println();
}
#endif

// ── Sampler task (Core 0) ─────────────────────────────────────────────────────

static void sampler_task(void*) {
    uint8_t active = 0;   // index of buffer currently being filled
    uint16_t idx   = 0;   // write position within the active buffer
    float t        = 0.0f;

    for (;;) {
        // Read the current rate (short critical section)
        xSemaphoreTake(g_fs_mutex, portMAX_DELAY);
        float fs = g_fs_current;
        xSemaphoreGive(g_fs_mutex);

        uint32_t t0 = micros();
        float raw = generate_sample(t);
        energy_model_record_active(micros() - t0);

        bool is_spike = false;
        float sample;
#if defined(SIGNAL_MODE) && SIGNAL_MODE == 1
        sample = raw + gaussian_noise(0.2f);
#elif !defined(SIGNAL_MODE) || SIGNAL_MODE == 2
        sample = inject_spike(raw, &is_spike);
#else  // SIGNAL_MODE == 0: clean
        sample = raw;
#endif

        // Pass current ring-buffer stats so Z-score can evaluate immediately.
        // Stats lag by one sample but are accurate enough for the proxy model.
        anomaly_process(sample, is_spike, ring_buffer_mean(), ring_buffer_std());

        buf_real[active][idx] = (double)sample;
        buf_imag[active][idx] = 0.0;
        ring_buffer_push(sample);

        // Timestamp the first sample of each FFT window for e2e latency measurement
        if (idx == 0) {
            g_window_start_us = esp_timer_get_time();
        }

        t   += 1.0f / fs;
        idx += 1;

        if (idx == FFT_N) {
            // Buffer full — hand it to the FFT task and flip to the other buffer.
            // Non-blocking send: if the queue is full (FFT still busy), skip this
            // window rather than blocking the sampler.
            FFTJob job = { active, fs };
            xQueueSend(g_fft_queue, &job, 0);
            active ^= 1;   // toggle: 0 → 1 → 0 → …
            idx     = 0;
        }

        // Sleep for one sample interval.
        // vTaskDelay has 1 ms resolution, which is fine for our rates (≥ 10 Hz = 100 ms).
        uint32_t ms = (uint32_t)(1000.0f / fs);
        if (ms == 0) ms = 1;
        uint32_t t1 = micros();
        vTaskDelay(pdMS_TO_TICKS(ms));
        energy_model_record_sleep(micros() - t1);
    }
}

// ── FFT task (Core 1) ─────────────────────────────────────────────────────────

static void fft_task(void*) {
    FFTJob job;

    for (;;) {
        // Block here until the sampler delivers a full buffer
        xQueueReceive(g_fft_queue, &job, portMAX_DELAY);

        ArduinoFFT<double> fft(
            buf_real[job.buf_idx],
            buf_imag[job.buf_idx],
            FFT_N,
            (double)job.fs
        );

        xSemaphoreTake(g_fft_capture_mutex, portMAX_DELAY);
        memcpy(g_last_fft_samples, buf_real[job.buf_idx], sizeof(g_last_fft_samples));
        g_last_fft_count = FFT_N;
        g_last_fft_fs    = job.fs;
        g_last_fft_seq++;
        uint32_t seq = g_last_fft_seq;
        xSemaphoreGive(g_fft_capture_mutex);

        fft.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        fft.compute(FFT_FORWARD);
        fft.complexToMagnitude();

        double dominant = fft.majorPeak();
        g_last_fft_dominant_hz = (float)dominant;

        // Nyquist minimum; clamp to [10, 100] Hz
        float new_fs = 2.0f * (float)dominant;
        if (new_fs < 10.0f)  new_fs = 10.0f;
        if (new_fs > 100.0f) new_fs = 100.0f;

        xSemaphoreTake(g_fs_mutex, portMAX_DELAY);
        g_fs_current = new_fs;
        xSemaphoreGive(g_fs_mutex);

        Serial.printf("[FFT]  dominant = %.2f Hz  →  fs updated to %.1f Hz\n",
                      (float)dominant, new_fs);

#if defined(PLOT_CAPTURE)
        emit_plot_capture(seq);
#endif
    }
}

// ── Public entry point ────────────────────────────────────────────────────────

void start_tasks() {
    g_fs_mutex  = xSemaphoreCreateMutex();
    g_fft_queue = xQueueCreate(1, sizeof(FFTJob));
    g_fft_capture_mutex = xSemaphoreCreateMutex();

    ring_buffer_init();

    // Sampler: Core 0, 4 KB stack, priority 1
    xTaskCreatePinnedToCore(sampler_task, "sampler", 4096, nullptr, 1, nullptr, 0);

    // FFT: Core 1, 8 KB stack (arduinoFFT uses more stack than a simple loop)
    xTaskCreatePinnedToCore(fft_task, "fft", 8192, nullptr, 1, nullptr, 1);

    // Aggregator: Core 0, wakes every 5 s to compute and print window mean
    start_aggregator_task();
}

bool copy_last_fft_window(double* dest,
                          uint16_t dest_len,
                          uint16_t* out_count,
                          float* out_fs,
                          uint32_t* out_seq) {
    if (!dest || dest_len < FFT_N || !out_count || !out_fs || !out_seq) {
        return false;
    }
    if (!g_fft_capture_mutex) return false;

    xSemaphoreTake(g_fft_capture_mutex, portMAX_DELAY);
    bool available = (g_last_fft_count == FFT_N);
    if (available) {
        memcpy(dest, g_last_fft_samples, sizeof(g_last_fft_samples));
        *out_count = g_last_fft_count;
        *out_fs    = g_last_fft_fs;
        *out_seq   = g_last_fft_seq;
    }
    xSemaphoreGive(g_fft_capture_mutex);
    return available;
}
