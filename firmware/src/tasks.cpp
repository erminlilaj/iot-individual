#include "tasks.h"
#include "sensor.h"
#include "aggregator.h"
#include "energy_model.h"
#include "anomaly.h"
#include <Arduino.h>
#include <arduinoFFT.h>
#include <freertos/queue.h>
#include <esp_timer.h>

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

// ── Job descriptor sent through the queue ────────────────────────────────────

struct FFTJob {
    uint8_t buf_idx;   // which buffer (0 or 1) is ready for FFT
    float   fs;        // sampling rate that was used to fill it
};

static QueueHandle_t g_fft_queue;   // depth 1 — one pending job at a time

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
    }
}

// ── Public entry point ────────────────────────────────────────────────────────

void start_tasks() {
    g_fs_mutex  = xSemaphoreCreateMutex();
    g_fft_queue = xQueueCreate(1, sizeof(FFTJob));

    ring_buffer_init();

    // Sampler: Core 0, 4 KB stack, priority 1
    xTaskCreatePinnedToCore(sampler_task, "sampler", 4096, nullptr, 1, nullptr, 0);

    // FFT: Core 1, 8 KB stack (arduinoFFT uses more stack than a simple loop)
    xTaskCreatePinnedToCore(fft_task, "fft", 8192, nullptr, 1, nullptr, 1);

    // Aggregator: Core 0, wakes every 5 s to compute and print window mean
    start_aggregator_task();
}
