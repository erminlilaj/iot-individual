#pragma once

/*
 * Phase 2 — Maximum Sampling Frequency
 *
 * Reports two different ceilings:
 *   1. raw virtual-sensor generation speed, with no scheduler delay
 *   2. current FreeRTOS-paced sampler speed, with vTaskDelay(1ms)
 *
 * WHY two values?
 *   A tight generate_sample() loop measures the raw computational ceiling of
 *   the virtual sensor path. The current firmware still schedules real
 *   sampling through tasks.cpp:sampler_task, where
 *   vTaskDelay(pdMS_TO_TICKS(ms)) and the ms=1 floor limit the running
 *   application path to about 1,000 Hz.
 *
 * WHY keep vTaskDelay(1ms)?
 *   tasks.cpp:sampler_task uses vTaskDelay(pdMS_TO_TICKS(ms)) with a hard
 *   floor of ms=1. The FreeRTOS tick period is 1 ms, so the task can never
 *   fire more than 1,000 times per second without changing the timing design.
 *
 * HOW it works:
 *   Use esp_timer_get_time() around a raw generate_sample() loop and around
 *   an analogRead(GPIO_1) + vTaskDelay(1ms) loop.
 */

// Runs the benchmark and prints results to Serial.
// Call once from setup() after Serial is initialised.
void run_sampling_benchmark();
