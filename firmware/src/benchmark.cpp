#include <Arduino.h>
#include <esp_timer.h>
#include "benchmark.h"
#include "sensor.h"

// ADC pin used purely for speed measurement — value is irrelevant.
// GPIO 1 is ADC1_CH0 on ESP32-S3 and is not used by any other peripheral
// on the Heltec WiFi LoRa 32 V3 (LoRa uses SPI, OLED uses I2C, Vext is GPIO 36).
static constexpr int BENCH_ADC_PIN = 1;

void run_sampling_benchmark() {
    const int RAW_NUM_SAMPLES = 20000;
    const int TASK_NUM_SAMPLES = 1000;
    const float nyquist_fs_hz = 10.0f;
    const float app_target_fs_hz = 40.0f;

    // Prevents dead-code elimination — compiler must keep the loop.
    volatile float raw_accumulator = 0.0f;
    uint32_t task_accumulator = 0;

    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║  Phase 2 — Max Sampling Frequency    ║");
    Serial.println("╚══════════════════════════════════════╝");
    Serial.println("  Measuring two ceilings:");
    Serial.println("  1) raw virtual-sensor loop, no scheduler delay");
    Serial.println("  2) FreeRTOS-paced application loop with vTaskDelay(1ms)");

    pinMode(BENCH_ADC_PIN, INPUT);

    int64_t raw_start_us = esp_timer_get_time();
    float t = 0.0f;
    const float raw_dt = 1.0f / 1000.0f;
    for (int i = 0; i < RAW_NUM_SAMPLES; i++) {
        raw_accumulator += generate_sample(t);
        t += raw_dt;
    }
    int64_t raw_elapsed_us = esp_timer_get_time() - raw_start_us;

    int64_t task_start_us = esp_timer_get_time();
    for (int i = 0; i < TASK_NUM_SAMPLES; i++) {
        task_accumulator += (uint32_t)analogRead(BENCH_ADC_PIN);
        vTaskDelay(pdMS_TO_TICKS(1));   // same 1 ms floor as sampler_task in tasks.cpp
    }
    int64_t task_elapsed_us = esp_timer_get_time() - task_start_us;

    float raw_elapsed_s      = (float)raw_elapsed_us / 1000000.0f;
    float raw_per_sample_us  = (float)raw_elapsed_us / (float)RAW_NUM_SAMPLES;
    float raw_fs_max         = (float)RAW_NUM_SAMPLES / raw_elapsed_s;
    float task_elapsed_s     = (float)task_elapsed_us / 1000000.0f;
    float task_per_sample_ms = (float)task_elapsed_us / (float)TASK_NUM_SAMPLES / 1000.0f;
    float task_fs_max        = (float)TASK_NUM_SAMPLES / task_elapsed_s;

    Serial.println();
    Serial.println("  ── Results ──────────────────────────");
    Serial.printf ("  Raw virtual samples        : %d\n", RAW_NUM_SAMPLES);
    Serial.printf ("  Raw loop elapsed           : %lld µs  (%.3f s)\n", raw_elapsed_us, raw_elapsed_s);
    Serial.printf ("  Raw time per sample        : %.2f µs\n", raw_per_sample_us);
    Serial.printf ("  Raw max generation rate    : %.0f Hz\n", raw_fs_max);
    Serial.println();
    Serial.printf ("  Task-paced ADC samples     : %d\n", TASK_NUM_SAMPLES);
    Serial.printf ("  Task-paced elapsed         : %lld µs  (%.3f s)\n", task_elapsed_us, task_elapsed_s);
    Serial.printf ("  Task-paced time/sample     : %.2f ms\n", task_per_sample_ms);
    Serial.printf ("  Task-paced max rate        : %.0f Hz\n", task_fs_max);
    Serial.println();
    Serial.println("  ── What this means ─────────────────");
    Serial.printf ("  Raw max answers: how fast can this board compute the virtual signal?\n");
    Serial.printf ("  Task-paced max answers: how fast does this firmware schedule samples today?\n");
    Serial.printf ("  Signal needs %.0f Hz by Nyquist; app uses %.0f Hz for conservative 8x margin.\n",
                   nyquist_fs_hz, app_target_fs_hz);
    Serial.printf ("  raw_fs_max / fs_app = %.0f / %.0f = %.0fx margin.\n",
                   raw_fs_max, app_target_fs_hz, raw_fs_max / app_target_fs_hz);
    Serial.printf ("  task_fs_max / fs_app = %.0f / %.0f = %.0fx margin.\n",
                   task_fs_max, app_target_fs_hz, task_fs_max / app_target_fs_hz);
    Serial.println();
    Serial.printf ("  (Checksums: raw=%.2f task=%lu — ignore)\n",
                   (float)raw_accumulator, (unsigned long)task_accumulator);
    Serial.println("════════════════════════════════════════");
}
