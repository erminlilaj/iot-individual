#include "aggregator.h"
#include "tasks.h"
#include "display.h"
#include "lorawan.h"
#include "mqtt_client.h"
#include "energy_model.h"
#include "anomaly.h"
#include "sensor.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <string.h>

// ── Ring buffer ───────────────────────────────────────────────────────────────

static const uint16_t RING_SIZE = 1000;   // covers 20 s at 50 Hz or 25 s at 40 Hz

static float        ring[RING_SIZE];
static uint16_t     head  = 0;    // next write position
static uint16_t     count = 0;    // number of valid entries (caps at RING_SIZE)
static SemaphoreHandle_t ring_mutex;

void ring_buffer_init() {
    ring_mutex = xSemaphoreCreateMutex();
}

void ring_buffer_push(float sample) {
    xSemaphoreTake(ring_mutex, portMAX_DELAY);
    ring[head] = sample;
    head = (head + 1) % RING_SIZE;
    if (count < RING_SIZE) count++;
    xSemaphoreGive(ring_mutex);
}

float ring_buffer_mean() {
    xSemaphoreTake(ring_mutex, portMAX_DELAY);
    uint16_t n = count;
    float sum  = 0.0f;
    // All valid entries occupy ring[0..n-1] while not yet full,
    // or ring[0..RING_SIZE-1] once full — either way sum the first n.
    for (uint16_t i = 0; i < n; i++) {
        sum += ring[i];
    }
    xSemaphoreGive(ring_mutex);
    return (n > 0) ? sum / (float)n : 0.0f;
}

float ring_buffer_mean_last(uint16_t sample_count) {
    xSemaphoreTake(ring_mutex, portMAX_DELAY);

    uint16_t available = count;
    uint16_t n = (sample_count < available) ? sample_count : available;
    float sum = 0.0f;

    if (n > 0) {
        // Walk backwards from the newest sample so the aggregation window
        // always covers the most recent fs * 5 seconds, not the whole history.
        for (uint16_t i = 0; i < n; i++) {
            int32_t idx = (int32_t)head - 1 - (int32_t)i;
            if (idx < 0) idx += RING_SIZE;
            sum += ring[(uint16_t)idx];
        }
    }

    xSemaphoreGive(ring_mutex);
    return (n > 0) ? sum / (float)n : 0.0f;
}

float ring_buffer_std() {
    xSemaphoreTake(ring_mutex, portMAX_DELAY);
    uint16_t n = count;
    float sum = 0.0f;
    for (uint16_t i = 0; i < n; i++) sum += ring[i];
    float mean = (n > 0) ? sum / (float)n : 0.0f;
    float var  = 0.0f;
    for (uint16_t i = 0; i < n; i++) {
        float d = ring[i] - mean;
        var += d * d;
    }
    xSemaphoreGive(ring_mutex);
    return (n > 1) ? sqrtf(var / (float)n) : 0.0f;
}

uint16_t ring_buffer_count() {
    xSemaphoreTake(ring_mutex, portMAX_DELAY);
    uint16_t n = count;
    xSemaphoreGive(ring_mutex);
    return n;
}

// ── Aggregator task ───────────────────────────────────────────────────────────

static void print_bonus_signal_summary(float fs, uint16_t target_samples) {
    float duty = energy_model_duty_cycle();
    float i_avg = duty * CURRENT_ACTIVE_MA + (1.0f - duty) * CURRENT_IDLE_MA;
    float scale = (fs > 0.0f) ? (BASELINE_FS_HZ / fs) : 1.0f;
    float duty_over = duty * scale;
    if (duty_over > 1.0f) duty_over = 1.0f;
    float i_over = duty_over * CURRENT_ACTIVE_MA + (1.0f - duty_over) * CURRENT_IDLE_MA;
    float savings = (i_over > 0.0f) ? (1.0f - i_avg / i_over) * 100.0f : 0.0f;
    unsigned int spike_pct = (strcmp(anomaly_signal_family(), "spikes") == 0) ? ANOMALY_SPIKE_PROB_PCT : 0;

    Serial.printf(
        "[BONUS-SIGNAL] mode=%s spike_pct=%u dominant=%.2f adaptive_fs=%.1f baseline_fs=%.1f window_n=%u baseline_window_n=%u duty_pct=%.4f i_avg_ma=%.3f savings_pct=%.2f variant=%s expected_fmax=%.1f\n",
        anomaly_signal_family(),
        spike_pct,
        g_last_fft_dominant_hz,
        fs,
        BASELINE_FS_HZ,
        target_samples,
        (unsigned int)lroundf(BASELINE_FS_HZ * 5.0f),
        duty * 100.0f,
        i_avg,
        savings,
        clean_signal_variant_label(),
        clean_signal_variant_expected_fmax_hz()
    );
}

static void aggregator_task(void*) {
    uint32_t window_count = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));   // 5-second aggregation window

        uint32_t t_start = micros();

        xSemaphoreTake(g_fs_mutex, portMAX_DELAY);
        float fs = g_fs_current;
        xSemaphoreGive(g_fs_mutex);

        uint16_t target_samples = (uint16_t)lroundf(fs * 5.0f);
        if (target_samples == 0) target_samples = 1;

        float mean = ring_buffer_mean_last(target_samples);
        uint16_t n = ring_buffer_count();
        if (n > target_samples) n = target_samples;

        window_count++;

        energy_model_print(fs);
        anomaly_print_stats(fs);
        print_bonus_signal_summary(fs, target_samples);
        anomaly_print_window_analysis(fs);
        display_update(fs, mean, n, lorawan_is_joined(), mqtt_is_connected(), window_count);
        lorawan_send(mean);
        mqtt_send(mean, window_count, n, fs, g_last_fft_dominant_hz);

        uint32_t proc_us = micros() - t_start;

        Serial.printf("[AGG]  win=%lu  mean=%+.4f  n=%u  fs=%.1f Hz  proc_us=%lu\n",
                      (unsigned long)window_count, mean, n, fs, (unsigned long)proc_us);
    }
}

void start_aggregator_task() {
    // Core 0. This task prints anomaly stats, updates display, and builds
    // communication payloads, so keep a generous stack margin.
    xTaskCreatePinnedToCore(aggregator_task, "aggregator", 16384, nullptr, 1, nullptr, 0);
}
