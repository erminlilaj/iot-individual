#include "display.h"
#include "tasks.h"
#include "mqtt_client.h"
#include "sensor.h"
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ── Heltec WiFi LoRa 32 V3 — OLED pin mapping (hardwired on board) ───────────
#define OLED_SDA  17
#define OLED_SCL  18
#define OLED_RST  21

// Hardware I2C — uses the ESP32-S3 I2C peripheral; immune to FreeRTOS preemption.
// SW_I2C bit-bangs GPIO in a ~90 ms loop; the FreeRTOS scheduler can preempt
// mid-transfer, producing malformed I2C that silently blanks the display.
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

static const char* signal_mode_label() {
#if defined(SIGNAL_MODE) && SIGNAL_MODE == 0
    return clean_signal_variant_label();
#elif defined(SIGNAL_MODE) && SIGNAL_MODE == 1
    return "noise";
#elif defined(SIGNAL_MODE) && SIGNAL_MODE == 2
  #if defined(SPIKE_PROB_PCT)
    switch (SPIKE_PROB_PCT) {
        case 1:  return "spike1";
        case 5:  return "spike5";
        case 10: return "spike10";
        default: return "spikes";
    }
  #else
    return "spikes";
  #endif
#else
    return "unknown";
#endif
}

void display_init() {
    // Vext (GPIO 36) must be HIGH to power the OLED on Heltec V3
    Serial.println("[OLED] Enabling Vext (GPIO 36)...");
    pinMode(36, OUTPUT);
    digitalWrite(36, HIGH);
    delay(100);   // give the Vext rail time to stabilise and charge pump to start

    // Drive RST manually before handing control to the library
    Serial.println("[OLED] RST pulse...");
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);

    Wire.begin(OLED_SDA, OLED_SCL);   // set correct pins before U8G2 takes over Wire
    bool ok = u8g2.begin();
    Serial.printf("[OLED] begin() = %s\n", ok ? "OK" : "FAILED");

    u8g2.setFont(u8g2_font_6x10_tf);   // 6x10 px per char -> 21 cols, 6 rows

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(18, 24, "IoT Pipeline");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(30, 42, "Adaptive Node");
    u8g2.drawStr(34, 57, "Booting...");
    u8g2.sendBuffer();

    Serial.println("[OLED] Splash sent");
}

void display_update(float fs,
                    float mean,
                    uint16_t buf_count,
                    bool lora_ok,
                    bool mqtt_ok,
                    uint32_t window_count) {
    char line[24];
    const char* mode = signal_mode_label();
    const char* lora_state = lora_ok ? "joined" : "join";
    int64_t rtt_ms = mqtt_last_rtt_ms();
    uint8_t page = (uint8_t)(window_count % 3);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    if (page == 0) {
        u8g2.drawStr(0, 10, "IoT Pipeline");
        snprintf(line, sizeof(line), "win:%lu", (unsigned long)window_count);
        u8g2.drawStr(86, 10, line);
        u8g2.drawHLine(0, 12, 128);

        snprintf(line, sizeof(line), "fs:   %4.1f Hz", fs);
        u8g2.drawStr(0, 26, line);

        snprintf(line, sizeof(line), "peak: %4.2f Hz", g_last_fft_dominant_hz);
        u8g2.drawStr(0, 40, line);

        snprintf(line, sizeof(line), "mode: %s", mode);
        u8g2.drawStr(0, 54, line);

        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(101, 62, "1/3");
    } else if (page == 1) {
        u8g2.drawStr(0, 10, "Window Stats");
        snprintf(line, sizeof(line), "win:%lu", (unsigned long)window_count);
        u8g2.drawStr(86, 10, line);
        u8g2.drawHLine(0, 12, 128);

        snprintf(line, sizeof(line), "mean: %+.4f", mean);
        u8g2.drawStr(0, 26, line);

        snprintf(line, sizeof(line), "n:    %u", buf_count);
        u8g2.drawStr(0, 40, line);

        snprintf(line, sizeof(line), "LoRa: %s", lora_state);
        u8g2.drawStr(0, 54, line);

        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(101, 62, "2/3");
    } else {
        u8g2.drawStr(0, 10, "Links");
        snprintf(line, sizeof(line), "win:%lu", (unsigned long)window_count);
        u8g2.drawStr(86, 10, line);
        u8g2.drawHLine(0, 12, 128);

        snprintf(line, sizeof(line), "MQTT: %s", mqtt_ok ? "OK" : "--");
        u8g2.drawStr(0, 26, line);

        if (rtt_ms >= 0) {
            snprintf(line, sizeof(line), "RTT:  %lld ms", (long long)rtt_ms);
        } else {
            snprintf(line, sizeof(line), "RTT:  --");
        }
        u8g2.drawStr(0, 40, line);

        snprintf(line, sizeof(line), "LoRa: %s", lora_state);
        u8g2.drawStr(0, 54, line);

        u8g2.setFont(u8g2_font_5x7_tf);
        u8g2.drawStr(101, 62, "3/3");
    }

    u8g2.sendBuffer();
}
