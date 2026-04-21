#include <Arduino.h>   // provides sinf(), M_PI, etc.
#include "sensor.h"

float generate_sample(float t_seconds) {
    // TWO_PI (= 2π ≈ 6.283) is already defined by Arduino.h — using it directly
    float c3hz = 2.0f * sinf(TWO_PI * 3.0f * t_seconds);  // amplitude 2, 3 Hz
    float c5hz = 4.0f * sinf(TWO_PI * 5.0f * t_seconds);  // amplitude 4, 5 Hz (dominant)
    return c3hz + c5hz;
}
