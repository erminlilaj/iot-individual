#include <Arduino.h>
#include <esp_timer.h>
#include "benchmark.h"

// ADC pin used purely for speed measurement — value is irrelevant.
// GPIO 1 is ADC1_CH0 on ESP32-S3 and is not used by any other peripheral
// on the Heltec WiFi LoRa 32 V3 (LoRa uses SPI, OLED uses I2C, Vext is GPIO 36).
static constexpr int BENCH_ADC_PIN = 1;

void run_sampling_benchmark() {
    const int NUM_SAMPLES = 1000;

    // Prevents dead-code elimination — compiler must keep the loop.
    uint32_t accumulator = 0;

    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║  Phase 2 — Max Sampling Frequency    ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.printf ("  Collecting %d samples with vTaskDelay(1ms) on GPIO %d...\n",
                   NUM_SAMPLES, BENCH_ADC_PIN);

    pinMode(BENCH_ADC_PIN, INPUT);

    int64_t start_us = esp_timer_get_time();

    for (int i = 0; i < NUM_SAMPLES; i++) {
        accumulator += (uint32_t)analogRead(BENCH_ADC_PIN);
        vTaskDelay(pdMS_TO_TICKS(1));   // same 1 ms floor as sampler_task in tasks.cpp
    }

    int64_t elapsed_us = esp_timer_get_time() - start_us;

    float elapsed_s     = (float)elapsed_us / 1000000.0f;
    float per_sample_ms = (float)elapsed_us / (float)NUM_SAMPLES / 1000.0f;
    float fs_max        = (float)NUM_SAMPLES / elapsed_s;

    Serial.println();
    Serial.println("  ── Results ──────────────────────────");
    Serial.printf ("  Samples collected  : %d\n",    NUM_SAMPLES);
    Serial.printf ("  Total elapsed time : %lld µs  (%.3f s)\n", elapsed_us, elapsed_s);
    Serial.printf ("  Time per sample    : %.2f ms\n", per_sample_ms);
    Serial.printf ("  Max sampling rate  : %.0f Hz\n", fs_max);
    Serial.println();
    Serial.println("  ── What this means ─────────────────");
    Serial.printf ("  Bottleneck: FreeRTOS scheduler tick (1 ms minimum delay).\n");
    Serial.printf ("  Signal needs only 10 Hz (Nyquist for 5 Hz).\n");
    Serial.printf ("  fs_max / fs_needed = %.0f / 10 = %.0fx oversampling.\n",
                   fs_max, fs_max / 10.0f);
    Serial.printf ("  Sampling-rate reduction potential: ~%.1f%%\n",
                   (1.0f - 10.0f / fs_max) * 100.0f);
    Serial.println();
    Serial.printf ("  (Checksum to prevent optimisation: %lu — ignore)\n", accumulator);
    Serial.println("════════════════════════════════════════");
}
