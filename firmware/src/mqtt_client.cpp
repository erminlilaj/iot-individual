#include "mqtt_client.h"
#include "config.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TOPIC_AVG  "eri/iot/average"
#define TOPIC_PING "eri/iot/ping"
#define TOPIC_PONG "eri/iot/pong"

static WiFiClient   s_wifi;
static PubSubClient s_mqtt(s_wifi);

// Ping-pong latency state
static volatile int64_t s_ping_us     = 0;
static volatile int64_t s_last_rtt_ms = -1;

// Volume accounting
static uint32_t s_send_count  = 0;
static uint32_t s_total_bytes = 0;

// ── MQTT callback ─────────────────────────────────────────────────────────────

static void on_message(char* topic, byte* /*payload*/, unsigned int /*len*/) {
    if (strcmp(topic, TOPIC_PONG) == 0) {
        int64_t rtt = (esp_timer_get_time() - s_ping_us) / 1000LL;
        s_last_rtt_ms = rtt;
        Serial.printf("[LATENCY] rtt_ms=%lld\n", (long long)rtt);
    }
}

// ── Connection helpers ────────────────────────────────────────────────────────

static bool ensure_connected() {
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
        return false;
    }
    if (!s_mqtt.connected()) {
        if (s_mqtt.connect(MQTT_CLIENT_ID)) {
            s_mqtt.subscribe(TOPIC_PONG);
            Serial.println("[MQTT] Reconnected to broker");
        }
        return s_mqtt.connected();
    }
    return true;
}

// ── Background loop task ──────────────────────────────────────────────────────

static void mqtt_loop_task(void*) {
    for (;;) {
        ensure_connected();
        if (s_mqtt.connected()) s_mqtt.loop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void mqtt_init() {
    Serial.printf("[WiFi] Connecting to '%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection failed — MQTT disabled for this session");
        return;
    }
    Serial.printf("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());

    s_mqtt.setServer(MQTT_BROKER_IP, MQTT_BROKER_PORT);
    s_mqtt.setCallback(on_message);
    s_mqtt.setKeepAlive(30);

    if (s_mqtt.connect(MQTT_CLIENT_ID)) {
        s_mqtt.subscribe(TOPIC_PONG);
        Serial.printf("[MQTT] Connected to %s:%d\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    } else {
        Serial.printf("[MQTT] Broker unreachable (rc=%d) — will retry in background\n",
                      s_mqtt.state());
    }

    // Core 0 — same core as sampler/aggregator; low priority so it yields readily
    xTaskCreatePinnedToCore(mqtt_loop_task, "mqtt_loop", 4096, nullptr, 1, nullptr, 0);
}

void mqtt_send(float mean) {
    if (!s_mqtt.connected()) return;

    // Publish aggregated average
    char buf[24];
    snprintf(buf, sizeof(buf), "%.4f", mean);
    if (!s_mqtt.publish(TOPIC_AVG, buf)) return;

    s_send_count++;
    uint32_t payload_len = (uint32_t)strlen(buf);
    s_total_bytes += payload_len;

    // Compare adaptive vs oversampled data volume.
    // Oversampled baseline: 1,000 Hz × 5 s = 5,000 raw float samples × 4 bytes each.
    uint32_t baseline_bytes = s_send_count * 5000u * sizeof(float);
    Serial.printf("[MQTT] #%lu avg=%s  payload=%lu B  total=%lu B  baseline=%lu B  ratio=%.0fx\n",
                  (unsigned long)s_send_count, buf,
                  (unsigned long)payload_len,
                  (unsigned long)s_total_bytes,
                  (unsigned long)baseline_bytes,
                  (float)baseline_bytes / (float)s_total_bytes);

    // Fire latency ping — edge server echoes it back on TOPIC_PONG
    s_ping_us = esp_timer_get_time();
    char ts[24];
    snprintf(ts, sizeof(ts), "%lld", (long long)s_ping_us);
    s_mqtt.publish(TOPIC_PING, ts);
}

bool    mqtt_is_connected() { return s_mqtt.connected(); }
int64_t mqtt_last_rtt_ms()  { return s_last_rtt_ms; }
