#pragma once
#include <stdint.h>

// Ring buffer — fixed-size circular array of float samples.
// Push is O(1); oldest sample is overwritten when full.
void ring_buffer_init();
void ring_buffer_push(float sample);
float ring_buffer_mean();
float ring_buffer_mean_last(uint16_t sample_count);
float ring_buffer_std();   // population std-dev of all buffered samples
uint16_t ring_buffer_count();

// Start the aggregator FreeRTOS task (call once from start_tasks()).
void start_aggregator_task();
