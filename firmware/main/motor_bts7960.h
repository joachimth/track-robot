/**
 * BTS7960 Motor Driver Controller
 *
 * Controls dual H-bridge motor drivers using PWM.
 * Supports direction control, speed ramping, and enable pins.
 */

#ifndef MOTOR_BTS7960_H
#define MOTOR_BTS7960_H

#include <stdbool.h>
#include "esp_err.h"

// Motor identifiers
typedef enum {
    MOTOR_LEFT = 0,
    MOTOR_RIGHT = 1,
    MOTOR_COUNT = 2
} motor_id_t;

/**
 * Initialize motor driver subsystem
 *
 * Configures GPIOs and LEDC PWM channels for both motors.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t motor_init(void);

/**
 * Set motor speed and direction
 *
 * @param motor Motor ID (MOTOR_LEFT or MOTOR_RIGHT)
 * @param speed Speed and direction (-1.0 to +1.0)
 *              Positive = forward, Negative = reverse, 0 = stop
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if out of range
 *
 * Note: Speed change is rate-limited by MOTOR_RAMP_RATE.
 *       Actual output will ramp to target over multiple calls.
 */
esp_err_t motor_set_speed(motor_id_t motor, float speed);

/**
 * Get current motor speed
 *
 * @param motor Motor ID
 * @return Current speed (-1.0 to +1.0), or 0.0 if invalid motor
 */
float motor_get_speed(motor_id_t motor);

/**
 * Emergency stop all motors
 *
 * Immediately sets all motor outputs to 0 (no ramping).
 *
 * @return ESP_OK on success
 */
esp_err_t motor_emergency_stop(void);

/**
 * Enable or disable motors
 *
 * Controls the EN pins on BTS7960 drivers.
 * When disabled, motor outputs are tri-stated (high impedance).
 *
 * @param enable true = enable outputs, false = disable (tri-state)
 *
 * @return ESP_OK on success
 *
 * Note: Only works if MOTOR_USE_ENABLE_PINS is true in config.h.
 *       If EN pins are tied to 5V, this function has no effect.
 */
esp_err_t motor_enable(bool enable);

/**
 * Check if motors are enabled
 *
 * @return true if motors are enabled, false otherwise
 */
bool motor_is_enabled(void);

/**
 * Update motor outputs (internal, called by main loop)
 *
 * Applies ramping and updates PWM duty cycles.
 * Should be called at regular intervals (e.g., 20ms).
 *
 * @return ESP_OK on success
 */
esp_err_t motor_update(void);

#endif // MOTOR_BTS7960_H
