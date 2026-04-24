#include <Arduino.h>   // provides sinf(), M_PI, etc.
#include "sensor.h"

const char* clean_signal_variant_label() {
#if CLEAN_SIGNAL_VARIANT == 1
    return "clean_b";
#elif CLEAN_SIGNAL_VARIANT == 2
    return "clean_c";
#else
    return "clean_a";
#endif
}

const char* clean_signal_variant_formula() {
#if CLEAN_SIGNAL_VARIANT == 1
    return "4*sin(2*pi*3*t)+2*sin(2*pi*9*t)";
#elif CLEAN_SIGNAL_VARIANT == 2
    return "2*sin(2*pi*2*t)+3*sin(2*pi*5*t)+1.5*sin(2*pi*7*t)";
#else
    return "2*sin(2*pi*3*t)+4*sin(2*pi*5*t)";
#endif
}

float clean_signal_variant_expected_fmax_hz() {
#if CLEAN_SIGNAL_VARIANT == 1
    return 9.0f;
#elif CLEAN_SIGNAL_VARIANT == 2
    return 7.0f;
#else
    return 5.0f;
#endif
}

float generate_sample(float t_seconds) {
    // TWO_PI (= 2*pi) is already defined by Arduino.h.
#if CLEAN_SIGNAL_VARIANT == 1
    float c3hz = 4.0f * sinf(TWO_PI * 3.0f * t_seconds);
    float c9hz = 2.0f * sinf(TWO_PI * 9.0f * t_seconds);
    return c3hz + c9hz;
#elif CLEAN_SIGNAL_VARIANT == 2
    float c2hz = 2.0f * sinf(TWO_PI * 2.0f * t_seconds);
    float c5hz = 3.0f * sinf(TWO_PI * 5.0f * t_seconds);
    float c7hz = 1.5f * sinf(TWO_PI * 7.0f * t_seconds);
    return c2hz + c5hz + c7hz;
#else
    float c3hz = 2.0f * sinf(TWO_PI * 3.0f * t_seconds);  // amplitude 2, 3 Hz
    float c5hz = 4.0f * sinf(TWO_PI * 5.0f * t_seconds);  // amplitude 4, 5 Hz (dominant)
    return c3hz + c5hz;
#endif
}
