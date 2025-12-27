/**
 * Differential Drive Mixer Implementation
 */

#include "mixer_diffdrive.h"
#include "config.h"
#include <math.h>

// Helper: Clamp value to range
static inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float mixer_apply_deadzone(float value, float deadzone) {
    if (fabsf(value) < deadzone) {
        return 0.0f;
    }

    // Scale value to maintain continuity at deadzone boundary
    // Maps [deadzone, 1.0] -> [0.0, 1.0] (or negative equivalent)
    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float abs_value = fabsf(value);
    float scaled = (abs_value - deadzone) / (1.0f - deadzone);
    return sign * scaled;
}

float mixer_apply_expo(float value, float expo) {
    if (expo <= 0.0f) {
        return value; // Invalid expo, return linear
    }

    float sign = (value > 0.0f) ? 1.0f : -1.0f;
    float abs_value = fabsf(value);
    float curved = powf(abs_value, expo);
    return sign * curved;
}

mixer_output_t mixer_mix(float throttle, float steering, bool slow_mode) {
    mixer_output_t output;

    // Apply deadzone
    throttle = mixer_apply_deadzone(throttle, STICK_DEADZONE);
    steering = mixer_apply_deadzone(steering, STICK_DEADZONE);

    // Apply expo curve
    throttle = mixer_apply_expo(throttle, STICK_EXPO);
    steering = mixer_apply_expo(steering, STICK_EXPO);

    // Apply speed limits
    throttle = clamp(throttle, -THROTTLE_MAX, THROTTLE_MAX);
    steering = clamp(steering, -STEERING_MAX, STEERING_MAX);

    // Tank mixing
    float left = throttle + steering;
    float right = throttle - steering;

    // Clamp to valid range
    left = clamp(left, -1.0f, 1.0f);
    right = clamp(right, -1.0f, 1.0f);

    // Apply slow mode limiter
    if (slow_mode) {
        left *= SLOW_MODE_FACTOR;
        right *= SLOW_MODE_FACTOR;
    }

    output.left = left;
    output.right = right;
    return output;
}
