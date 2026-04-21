#pragma once

/*
 * Virtual sensor: simulates s(t) = 2·sin(2π·3·t) + 4·sin(2π·5·t)
 *
 * WHY a virtual sensor?
 *   We have no physical hardware that produces a known sinusoidal signal.
 *   By generating it in software we can verify that the FFT (Phase 3)
 *   correctly detects the two frequency components (3 Hz and 5 Hz).
 *   Later the call to generate_sample() can be replaced with analogRead()
 *   for a real sensor — the rest of the system stays unchanged.
 *
 * HOW to advance time:
 *   Keep a float t = 0 and add (1.0f / fs) after every call.
 *   Example at 100 Hz:  t += 0.01f;
 *   Example at  10 Hz:  t += 0.10f;
 */

// Returns the signal value at time t_seconds.
// Expected output range: approximately [-6.0, +6.0]
float generate_sample(float t_seconds);
