#include "lorawan.h"
#include "lorawan_keys.h"
#include "tasks.h"
#include <RadioLib.h>
#include <Arduino.h>
#include <esp_timer.h>

// SX1262 wiring on Heltec WiFi LoRa 32 V3 (hardwired on PCB)
// Module(NSS, DIO1, RST, BUSY)
static SX1262 radio = new Module(8, 14, 12, 13);
static LoRaWANNode node(&radio, &EU868);

static bool     g_joined       = false;
static uint32_t s_uplink_count = 0;
static uint32_t s_total_bytes  = 0;

void lorawan_init() {
    Serial.println("[LoRa] Initialising SX1262...");

    int16_t state = radio.begin();
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] radio.begin() failed: %d\n", state);
        return;
    }

    // Maximise join range: SF12, 125 kHz, 4/8 CR, +22 dBm (SX1262 max)
    radio.setSpreadingFactor(12);
    radio.setBandwidth(125.0);
    radio.setCodingRate(8);
    radio.setOutputPower(22);
    Serial.println("[LoRa] TX set to SF12 BW125 CR4/8 +22dBm");

    // beginOTAA stores keys only (returns void); activateOTAA() does the actual join
    node.beginOTAA(LORAWAN_JOIN_EUI, LORAWAN_DEV_EUI,
                   (uint8_t*)LORAWAN_NWK_KEY, (uint8_t*)LORAWAN_APP_KEY);

    // Each activateOTAA() attempt increments devNonce by 1 internally.
    // Retry up to 20 times to skip past nonces TTN has already seen.
    Serial.println("[LoRa] Starting OTAA join (retries up to 60)...");
    for (int attempt = 1; attempt <= 60; attempt++) {
        state = node.activateOTAA();
        if (state == RADIOLIB_ERR_NONE) break;
        Serial.printf("[LoRa] attempt %d failed: %d\n", attempt, state);
        delay(500);
    }
    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LoRa] OTAA join failed after all retries: %d\n", state);
        return;
    }

    g_joined = true;
    Serial.println("[LoRa] OTAA join successful — uplinks enabled");
}

bool lorawan_is_joined() { return g_joined; }

void lorawan_send(float mean) {
    if (!g_joined) return;

    // Encode mean as int16 × 100 → 2 bytes, covers ±327.67 (signal max ±6)
    int16_t payload = (int16_t)(mean * 100.0f);

    uint8_t nbytes = sizeof(payload);   // always 2
    int16_t state  = node.sendReceive((uint8_t*)&payload, nbytes, 1);
    if (state == RADIOLIB_ERR_NONE || state == RADIOLIB_ERR_RX_TIMEOUT || state == -1116) {
        s_uplink_count++;
        s_total_bytes += nbytes;

        // Oversampled baseline: raw 4-byte floats at 1,000 Hz over the same elapsed time
        uint32_t elapsed_s        = s_uplink_count * 5;
        uint32_t oversampled_bytes = (uint32_t)(1000.0f * elapsed_s) * sizeof(float);

        int64_t latency_ms = (esp_timer_get_time() - g_window_start_us) / 1000LL;
        Serial.printf("[LoRa] uplink #%lu  mean=%+.4f  encoded=%d"
                      "  cumulative=%lu B  vs_oversampled=%lu B  ratio=%.0fx"
                      "  e2e_latency=%lld ms\n",
                      (unsigned long)s_uplink_count, mean, payload,
                      (unsigned long)s_total_bytes,
                      (unsigned long)oversampled_bytes,
                      (float)oversampled_bytes / (float)s_total_bytes,
                      (long long)latency_ms);
    } else {
        Serial.printf("[LoRa] send failed: %d\n", state);
    }
}
