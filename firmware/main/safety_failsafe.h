/**
 * Safety and Failsafe Controller
 *
 * Monitors control sources and implements safety features:
 * - Emergency stop (latched until cleared)
 * - Failsafe timeout (stop if no commands received)
 * - Control source arbitration (priority-based selection)
 */

#ifndef SAFETY_FAILSAFE_H
#define SAFETY_FAILSAFE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

/**
 * Control command structure (shared by all control sources)
 */
typedef struct {
    float throttle;          // -1.0 to +1.0
    float steering;          // -1.0 to +1.0
    bool estop;              // Emergency stop flag
    bool slow_mode;          // Slow mode flag
    uint32_t timestamp;      // millis() when command was set
    control_source_t source; // Which source generated this command
} control_command_t;

/**
 * Safety status
 */
typedef enum {
    SAFETY_STATUS_OK,        // Normal operation
    SAFETY_STATUS_ESTOP,     // Emergency stop active
    SAFETY_STATUS_FAILSAFE   // Failsafe triggered (timeout)
} safety_status_t;

/**
 * Initialize safety subsystem
 */
void safety_init(void);

/**
 * Update control command from a source
 *
 * Call this from each control source (PS4, Serial, HTTP) when
 * a new command is received.
 *
 * @param source Which control source is updating
 * @param throttle Throttle value (-1.0 to +1.0)
 * @param steering Steering value (-1.0 to +1.0)
 * @param estop Emergency stop flag
 * @param slow_mode Slow mode flag
 */
void safety_update_command(control_source_t source, float throttle,
                           float steering, bool estop, bool slow_mode);

/**
 * Get the active control command
 *
 * Returns the command from the highest-priority active source,
 * with safety checks applied (failsafe, e-stop).
 *
 * @param[out] throttle Output throttle value
 * @param[out] steering Output steering value
 * @param[out] slow_mode Output slow mode flag
 *
 * @return Active control source, or CONTROL_SOURCE_NONE if none active
 */
control_source_t safety_get_active_command(float *throttle, float *steering,
                                            bool *slow_mode);

/**
 * Get current safety status
 *
 * @return Safety status (OK, ESTOP, FAILSAFE)
 */
safety_status_t safety_get_status(void);

/**
 * Trigger emergency stop
 *
 * Sets e-stop flag and stops all motors.
 * If ESTOP_LATCH is true, remains active until safety_clear_estop() is called.
 */
void safety_trigger_estop(void);

/**
 * Clear emergency stop
 *
 * Allows motors to resume operation.
 * Has no effect if e-stop is not latched (ESTOP_LATCH = false).
 */
void safety_clear_estop(void);

/**
 * Check if emergency stop is active
 *
 * @return true if e-stop is active, false otherwise
 */
bool safety_is_estop_active(void);

/**
 * Update safety monitoring (call periodically from main loop)
 *
 * Checks for:
 * - Failsafe timeout (no commands from any source)
 * - E-stop conditions
 *
 * Should be called at regular intervals (e.g., every 20ms)
 */
void safety_update(void);

/**
 * Get active control source name (for debugging/logging)
 *
 * @param source Control source enum
 * @return String name of source ("PS4", "HTTP", "Serial", "None")
 */
const char *safety_source_name(control_source_t source);

#endif // SAFETY_FAILSAFE_H
