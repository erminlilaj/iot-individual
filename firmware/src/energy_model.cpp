#include "energy_model.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

// ── Accumulator state ────────────────────────────────────────────────────────
//
// Both accumulators are written only from sampler_task (Core 0) and read from
// aggregator_task (also Core 0).  portMUX ensures the read-pair is atomic.

static portMUX_TYPE  s_mux         = portMUX_INITIALIZER_UNLOCKED;
static uint64_t      s_active_us   = 0;
static uint64_t      s_sleep_us    = 0;

void energy_model_init() {
    s_active_us = 0;
    s_sleep_us  = 0;
}

void energy_model_record_active(uint32_t us) {
    portENTER_CRITICAL(&s_mux);
    s_active_us += us;
    portEXIT_CRITICAL(&s_mux);
}

void energy_model_record_sleep(uint32_t us) {
    portENTER_CRITICAL(&s_mux);
    s_sleep_us += us;
    portEXIT_CRITICAL(&s_mux);
}

float energy_model_duty_cycle() {
    portENTER_CRITICAL(&s_mux);
    uint64_t a = s_active_us;
    uint64_t s = s_sleep_us;
    portEXIT_CRITICAL(&s_mux);

    uint64_t total = a + s;
    if (total == 0) return 0.0f;
    return (float)a / (float)total;
}

// Battery life estimate using the weighted-average current model:
//   I_avg = D * I_active + (1-D) * I_sleep
//   T     = capacity_mah / I_avg
float energy_model_battery_hours(float capacity_mah) {
    float d     = energy_model_duty_cycle();
    float i_avg = d * CURRENT_ACTIVE_MA + (1.0f - d) * CURRENT_IDLE_MA;
    if (i_avg <= 0.0f) return 0.0f;
    return capacity_mah / i_avg;
}

void energy_model_print(float current_fs_hz) {
    float d      = energy_model_duty_cycle();
    float i_avg  = d * CURRENT_ACTIVE_MA + (1.0f - d) * CURRENT_IDLE_MA;
    float hours  = energy_model_battery_hours(1000.0f);  // assume 1000 mAh cell

    // Compare against the app's original 100 Hz oversampled baseline.
    // Sampling work is assumed to scale linearly with sample rate.
    float scale   = (current_fs_hz > 0.0f) ? (BASELINE_FS_HZ / current_fs_hz) : 1.0f;
    float d_over  = d * scale;
    if (d_over > 1.0f) d_over = 1.0f;

    float i_over  = d_over * CURRENT_ACTIVE_MA + (1.0f - d_over) * CURRENT_IDLE_MA;
    float h_over  = (i_over > 0.0f) ? (1000.0f / i_over) : 0.0f;
    float savings  = (1.0f - i_avg / i_over) * 100.0f;

    portENTER_CRITICAL(&s_mux);
    uint64_t a = s_active_us;
    uint64_t s = s_sleep_us;
    portEXIT_CRITICAL(&s_mux);

    Serial.printf(
        "[ENERGY] proxy: duty=%.4f%%  I_avg=%.3f mA  batt=%.0f h"
        "  | baseline@%.0fHz: duty=%.4f%%  I_avg=%.3f mA  batt=%.0f h"
        "  | savings=%.2f%%  (active=%llu µs  wait=%llu µs)\n",
        d * 100.0f, i_avg, hours,
        BASELINE_FS_HZ, d_over * 100.0f, i_over, h_over,
        savings, a, s
    );
}
