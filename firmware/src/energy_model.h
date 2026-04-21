#pragma once
#include <stdint.h>

// Proxy current model based on external INA219 measurements of this DUT setup.
// The current firmware keeps WiFi/MQTT alive and only allows automatic light
// sleep, so deep-sleep numbers would be misleading here.
static constexpr float CURRENT_ACTIVE_MA = 177.0f;  // observed burst current
static constexpr float CURRENT_IDLE_MA   =  78.0f;  // observed steady idle current
static constexpr float BASELINE_FS_HZ    = 100.0f;  // oversampled baseline used by the app

void  energy_model_init();
void  energy_model_record_active(uint32_t us);  // µs spent computing a sample
void  energy_model_record_sleep(uint32_t us);   // µs spent waiting between samples
float energy_model_duty_cycle();                // 0.0–1.0
float energy_model_battery_hours(float capacity_mah);
void  energy_model_print(float current_fs_hz);  // prints [ENERGY] line
