#pragma once
#include <stdint.h>

// Connect to WiFi and start the MQTT loop task.
// Call once from setup() before lorawan_init().
void mqtt_init();

// Publish the 5-second window average to the edge server and fire a latency ping.
// Returns silently if the broker connection is not up.
void mqtt_send(float mean,
               uint32_t window_id,
               uint16_t sample_count,
               float fs_hz,
               float dominant_hz);

bool    mqtt_is_connected();  // true when the MQTT broker connection is live
int64_t mqtt_last_rtt_ms();   // most recent ping-pong RTT in ms (-1 if none yet)
