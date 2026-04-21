#include "ina219.h"
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// Dedicated I2C bus (Wire1) so the INA219 does not conflict with the OLED
// which owns Wire on GPIO17/18.
#define INA219_SDA 41
#define INA219_SCL 42

static Adafruit_INA219 g_ina219;
static double          s_energy_mJ = 0.0;
static uint32_t        s_last_ms   = 0;

static void ina219_task(void*) {
    s_last_ms = millis();
    for (;;) {
        uint32_t now   = millis();
        uint32_t dt_ms = now - s_last_ms;
        s_last_ms      = now;

        float bus_V    = g_ina219.getBusVoltage_V();
        float shunt_mV = g_ina219.getShuntVoltage_mV();
        float mA       = g_ina219.getCurrent_mA();
        float mW       = g_ina219.getPower_mW();
        float load_V   = bus_V + shunt_mV / 1000.0f;

        // Accumulate energy: E(mJ) += P(mW) × Δt(s)
        s_energy_mJ += (double)mW * dt_ms / 1000.0;

        // Format matches RE_POWER in tools/serial_plotter.py:
        // RE_POWER = re.compile(r'\[POWER\].*mA=([\d.]+).*mW=([\d.]+)')
        Serial.printf("[POWER] bus_V=%.3f mA=%.2f mW=%.2f energy_mJ=%.1f\n",
                      load_V, mA, mW, s_energy_mJ);

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void ina219_init() {
    Wire1.begin(INA219_SDA, INA219_SCL);
    if (!g_ina219.begin(&Wire1)) {
        Serial.println("[INA219] Sensor not found on GPIO41/42 — power measurement disabled");
        return;
    }
    Serial.println("[INA219] Sensor found on GPIO41/42 — starting power measurement task");
    xTaskCreatePinnedToCore(ina219_task, "ina219", 2048, nullptr, 1, nullptr, 1);
}
