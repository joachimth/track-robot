/**
 * Differential Drive Mixer
 *
 * Converts (throttle, steering) commands to individual (left, right) motor speeds.
 * Implements tank-style mixing with deadzone, expo, and speed limiting.
 */

#ifndef MIXER_DIFFDRIVE_H
#define MIXER_DIFFDRIVE_H

#include <stdbool.h>

/**
 * Mixer output structure
 */
typedef struct {
    float left;   // Left motor speed (-1.0 to +1.0)
    float right;  // Right motor speed (-1.0 to +1.0)
} mixer_output_t;

/**
 * Mix throttle and steering into left/right motor commands
 *
 * Uses tank mixing algorithm:
 *   left  = throttle + steering
 *   right = throttle - steering
 *
 * Then applies:
 *   - Deadzone (ignore inputs below threshold)
 *   - Expo curve (reduce sensitivity near center)
 *   - Clamping to [-1.0, +1.0]
 *   - Speed limiting (slow mode, max speed)
 *
 * @param throttle Forward/reverse input (-1.0 to +1.0)
 * @param steering Left/right turn input (-1.0 to +1.0)
 * @param slow_mode Enable slow mode (limits max speed)
 *
 * @return mixer_output_t containing left and right motor speeds
 */
mixer_output_t mixer_mix(float throttle, float steering, bool slow_mode);

/**
 * Apply deadzone to input value
 *
 * Returns 0.0 if abs(value) < deadzone, otherwise returns value
 * scaled to maintain continuity at deadzone boundary.
 *
 * @param value Input value (-1.0 to +1.0)
 * @param deadzone Deadzone threshold (0.0 to 1.0)
 *
 * @return Processed value with deadzone applied
 */
float mixer_apply_deadzone(float value, float deadzone);

/**
 * Apply expo curve to input value
 *
 * Reduces sensitivity near center, increases at extremes.
 * Formula: sign(x) * |x|^expo
 *
 * @param value Input value (-1.0 to +1.0)
 * @param expo Expo factor (1.0 = linear, 2.0 = quadratic)
 *
 * @return Processed value with expo applied
 */
float mixer_apply_expo(float value, float expo);

#endif // MIXER_DIFFDRIVE_H
