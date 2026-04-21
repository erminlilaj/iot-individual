#pragma once
#include <stdint.h>

// Initialise the SSD1306 OLED and show a startup splash.
// Call once from setup() before starting FreeRTOS tasks.
void display_init();

// Refresh the display with the latest pipeline state.
// Called by the aggregator task every 5 seconds.
void display_update(float fs,
                    float mean,
                    uint16_t buf_count,
                    bool lora_ok,
                    bool mqtt_ok,
                    uint32_t window_count);
