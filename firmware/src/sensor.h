#pragma once

/*
 * Virtual sensor: simulates one of the compile-time signal variants.
 *
 * WHY a virtual sensor?
 *   We have no physical hardware that produces a known sinusoidal signal.
 *   By generating it in software we can verify that the FFT (Phase 3)
 *   correctly detects the intended frequency content.
 *   Later the call to generate_sample() can be replaced with analogRead()
 *   for a real sensor — the rest of the system stays unchanged.
 *
 * HOW to advance time:
 *   Keep a float t = 0 and add (1.0f / fs) after every call.
 *   Example at 100 Hz:  t += 0.01f;
 *   Example at  10 Hz:  t += 0.10f;
 */

#ifndef CLEAN_SIGNAL_VARIANT
  #define CLEAN_SIGNAL_VARIANT 0
#endif

// Returns the signal value at time t_seconds.
// Signal variants:
//   0 -> 2*sin(2*pi*3*t) + 4*sin(2*pi*5*t)
//   1 -> 4*sin(2*pi*3*t) + 2*sin(2*pi*9*t)
//   2 -> 2*sin(2*pi*2*t) + 3*sin(2*pi*5*t) + 1.5*sin(2*pi*7*t)
float generate_sample(float t_seconds);

// Human-readable metadata for logs, docs, and OLED pages.
const char* clean_signal_variant_label();
const char* clean_signal_variant_formula();
float clean_signal_variant_expected_fmax_hz();
