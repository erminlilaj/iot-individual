#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

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

// Copy the most recent raw FFT window captured before the transform runs.
// Returns false until the first full window has been processed.
bool copy_last_fft_window(double* dest,
                          uint16_t dest_len,
                          uint16_t* out_count,
                          float* out_fs,
                          uint32_t* out_seq);
