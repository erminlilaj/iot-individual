#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Start the sampler task (Core 0) and FFT task (Core 1).
void start_tasks();

// Current adaptive sampling rate in Hz.
// Protected by g_fs_mutex — always lock before reading or writing.
extern volatile float g_fs_current;
extern volatile float g_last_fft_dominant_hz;
extern SemaphoreHandle_t g_fs_mutex;

// Timestamp (µs since boot) of the first sample in the current FFT window.
// Written by sampler_task; read by lorawan_send to compute end-to-end latency.
extern volatile int64_t g_window_start_us;
