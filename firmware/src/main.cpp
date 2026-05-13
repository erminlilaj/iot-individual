#include <Arduino.h>
#include "benchmark.h"
#include "tasks.h"
#include "display.h"
#include "lorawan.h"
#include "mqtt_client.h"
#include "ina219.h"
#include "energy_model.h"
#include "anomaly.h"

// ── Phase 4: FreeRTOS Adaptive Sampling ────────────────────────────────────
//
// HOW TO USE:
//   1. Flash this firmware
//   2. Open Serial Monitor at 115200 baud
//   3. Watch for [FFT] lines — after the first window (2.56 s) the rate
//      drops from 50 Hz to ~40 Hz and then stays there.

#include "esp_pm.h"

void setup() {
    Serial.begin(115200);
    // Wait up to 2 s for a host to open the port; continue regardless
    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) delay(10);
    delay(500);

    // Enable Automatic Light Sleep
    esp_pm_config_esp32s3_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 10,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);
    Serial.println("[PM] Automatic light sleep enabled");

    display_init();
    Serial.println("[INA219] disabled for this capture run");
    energy_model_init();
    anomaly_init();
    anomaly_print_config();

    // Phase 2 result (one-shot, ~10 ms)
    run_sampling_benchmark();

    // Start FreeRTOS tasks so the display and sampler are live immediately.
    // lorawan_init() can block up to ~120 s (20 OTAA retries × 6 s each).
    // mqtt_init() can block up to ~15 s waiting for WiFi.
    // Both run after start_tasks() so other tasks stay alive during init.
    Serial.println("\nStarting FreeRTOS tasks (sampler + FFT + aggregator)...");
    start_tasks();
    Serial.println("Tasks running. Waiting for first FFT window (~2.6 s)...\n");

    mqtt_init();         // WiFi + MQTT; starts mqtt_loop_task when connected
    lorawan_init();      // may block a long time; FreeRTOS tasks run concurrently
}

void loop() {
    // Print the current adaptive rate every 5 seconds so you can watch it converge
    xSemaphoreTake(g_fs_mutex, portMAX_DELAY);
    float fs = g_fs_current;
    xSemaphoreGive(g_fs_mutex);

    Serial.printf("[main] Current fs = %.1f Hz\n", fs);
    delay(5000);
}
