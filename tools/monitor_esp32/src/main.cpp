/*
 * INA219 Power Monitor — Heltec WiFi LoRa 32 V3 (ESP32-S3)
 *
 * Outputs two line types every 200 ms on the same serial port:
 *
 *   [POWER] bus_V=X mA=Y mW=Z phase=P energy_mJ=E
 *       → parsed by tools/serial_plotter.py (RE_POWER regex)
 *
 *   X\tY\tZ\tP\tE
 *       → parsed by BetterSerialPlotter (pure tab-separated floats)
 *         Channels: [0]bus_V [1]current_mA [2]power_mW [3]phase_id [4]energy_mJ
 *
 * Keep the runtime stream numeric-only after startup so BetterSerialPlotter
 * can lock onto the channel data reliably.
 *
 * Wiring:
 *   INA219 VCC  → 3.3 V
 *   INA219 GND  → GND
 *   INA219 SDA  → GPIO 41  (Wire1)
 *   INA219 SCL  → GPIO 42  (Wire1)
 *   INA219 VIN+ → 5 V supply (+)
 *   INA219 VIN− → DUT VBUS / 5 V input
 *   Common GND between supply, INA219, DUT, and this board.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

// ── I2C (Wire1 keeps OLED on Wire untouched if present) ───────────────────────
static constexpr uint8_t INA_SDA = 41;
static constexpr uint8_t INA_SCL = 42;

// ── LED — Heltec V3 has no bare GPIO LED; use LED_BUILTIN if defined ──────────
#ifdef LED_BUILTIN
static constexpr uint8_t LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t LED_PIN = 35;   // fallback — change if yours differs
#endif

// ── Measurement interval ──────────────────────────────────────────────────────
static constexpr uint32_t SAMPLE_MS = 200;

// ── Phase thresholds (mA) ─────────────────────────────────────────────────────
static constexpr float THRESH_WIFI_IDLE = 80.0f;
static constexpr float THRESH_ACTIVE   = 170.0f;
static constexpr float THRESH_TX       = 220.0f;

// ── Phase enum ────────────────────────────────────────────────────────────────
enum Phase { IDLE = 0, WIFI_IDLE = 1, ACTIVE = 2, TRANSMIT = 3 };
static const char* PHASE_NAMES[] = { "IDLE", "WIFI_IDLE", "ACTIVE", "TX" };

static Phase classify(float mA) {
    float a = (mA < 0) ? -mA : mA;   // absolute value (handles reversed shunt)
    if (a > THRESH_TX)        return TRANSMIT;
    if (a > THRESH_ACTIVE)    return ACTIVE;
    if (a > THRESH_WIFI_IDLE) return WIFI_IDLE;
    return IDLE;
}

// ── LED feedback ──────────────────────────────────────────────────────────────
static bool     s_led     = false;
static uint32_t s_blink_t = 0;

static void led_update(Phase p) {
    uint32_t now = millis();
    switch (p) {
        case IDLE:
            digitalWrite(LED_PIN, LOW);
            break;
        case WIFI_IDLE:
            if (now - s_blink_t >= 1000) {
                s_led = !s_led;
                digitalWrite(LED_PIN, s_led);
                s_blink_t = now;
            }
            break;
        case ACTIVE:
            digitalWrite(LED_PIN, HIGH);
            break;
        case TRANSMIT:
            if (now - s_blink_t >= 100) {
                s_led = !s_led;
                digitalWrite(LED_PIN, s_led);
                s_blink_t = now;
            }
            break;
    }
}

// ── Globals ───────────────────────────────────────────────────────────────────
static Adafruit_INA219 s_ina;
static double          s_energy_mJ = 0.0;
static uint32_t        s_last_ms   = 0;

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire1.begin(INA_SDA, INA_SCL);
    if (!s_ina.begin(&Wire1)) {
        Serial.println("[INA219] ERROR: not found on GPIO41/42 — check wiring");
        for (;;) {                          // rapid blink = wiring fault
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
            delay(100);
        }
    }

    Serial.println("[INA219] Monitor ready — DUT power measurement active");

    s_last_ms = millis();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    uint32_t now = millis();
    if (now - s_last_ms < SAMPLE_MS) return;

    uint32_t dt_ms = now - s_last_ms;
    s_last_ms      = now;

    float bus_V    = s_ina.getBusVoltage_V();
    float shunt_mV = s_ina.getShuntVoltage_mV();
    float mA       = s_ina.getCurrent_mA();
    float mW       = s_ina.getPower_mW();
    float load_V   = bus_V + shunt_mV / 1000.0f;

    s_energy_mJ += (double)mW * dt_ms / 1000.0;

    Phase p = classify(mA);
    led_update(p);

    Serial.printf("%.3f\t%.2f\t%.2f\t%d\t%.1f\n",
                  bus_V, fabsf(mA), mW, (int)p, s_energy_mJ);
}
