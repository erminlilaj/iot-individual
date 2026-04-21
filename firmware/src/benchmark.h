#pragma once

/*
 * Phase 2 — Maximum Sampling Frequency
 *
 * Reports the highest rate the FreeRTOS sampler task can sustain: ~1,000 Hz.
 *
 * WHY vTaskDelay(1ms) and not a bare analogRead() loop?
 *   tasks.cpp:sampler_task uses vTaskDelay(pdMS_TO_TICKS(ms)) with a hard
 *   floor of ms=1. The FreeRTOS tick period is 1 ms, so the task can never
 *   fire more than 1,000 times per second regardless of ADC speed. A bare
 *   analogRead() loop (~16,662 Hz) measures ADC hardware throughput — a
 *   ceiling the running scheduler can never reach and therefore not useful
 *   as "maximum sampling frequency of this system".
 *
 * HOW it works:
 *   Call analogRead(GPIO_1) + vTaskDelay(1ms) NUM_SAMPLES times with
 *   esp_timer_get_time() (µs precision) before and after.
 *   Divide sample count by elapsed seconds → Hz.  Expected result: ~1,000 Hz.
 */

// Runs the benchmark and prints results to Serial.
// Call once from setup() after Serial is initialised.
void run_sampling_benchmark();
