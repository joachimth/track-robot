/**
 * PS4 Controller Input (Bluetooth LE)
 *
 * IMPORTANT: ESP32-C5 only supports Bluetooth LE, NOT Bluetooth Classic.
 * PS3 controllers use Bluetooth Classic and will NOT work with ESP32-C5.
 * This module implements PS4 controller support via BLE HID profile.
 *
 * To use PS3 controllers, you must use ESP32 or ESP32-S3 hardware instead.
 */

#ifndef CONTROLLER_PS4_H
#define CONTROLLER_PS4_H

#include "esp_err.h"
#include <stdbool.h>

// PS4 Controller Buttons (bitmask)
#define PS4_BUTTON_TRIANGLE  (1 << 0)
#define PS4_BUTTON_CIRCLE    (1 << 1)
#define PS4_BUTTON_CROSS     (1 << 2)
#define PS4_BUTTON_SQUARE    (1 << 3)
#define PS4_BUTTON_L1        (1 << 4)
#define PS4_BUTTON_R1        (1 << 5)
#define PS4_BUTTON_L2        (1 << 6)
#define PS4_BUTTON_R2        (1 << 7)
#define PS4_BUTTON_SHARE     (1 << 8)
#define PS4_BUTTON_OPTIONS   (1 << 9)
#define PS4_BUTTON_START     (1 << 9) // Alias for OPTIONS
#define PS4_BUTTON_L3        (1 << 10)
#define PS4_BUTTON_R3        (1 << 11)
#define PS4_BUTTON_PS        (1 << 12)
#define PS4_BUTTON_TOUCHPAD  (1 << 13)

// PS4 Analog Sticks
typedef enum {
    PS4_ANALOG_STICK_LX = 0,
    PS4_ANALOG_STICK_LY = 1,
    PS4_ANALOG_STICK_RX = 2,
    PS4_ANALOG_STICK_RY = 3
} ps4_analog_stick_t;

/**
 * Initialize PS4 controller module
 *
 * Initializes Bluetooth LE stack and starts advertising for pairing.
 *
 * @return ESP_OK on success, error code otherwise
 *
 * Note: This is currently a STUB implementation. Full BLE HID support
 *       requires ESP-IDF Bluetooth components and HID profile implementation.
 *       See docs/architecture.md for implementation notes.
 */
esp_err_t controller_ps4_init(void);

/**
 * Update PS4 controller state (call periodically)
 *
 * Reads latest controller input and updates safety subsystem.
 *
 * @return ESP_OK if controller connected, ESP_ERR_NOT_FOUND if disconnected
 */
esp_err_t controller_ps4_update(void);

/**
 * Check if PS4 controller is connected
 *
 * @return true if connected, false otherwise
 */
bool controller_ps4_is_connected(void);

/**
 * Get button state
 *
 * @param button Button bitmask (e.g., PS4_BUTTON_CROSS)
 * @return true if button pressed, false otherwise
 */
bool controller_ps4_get_button(uint16_t button);

/**
 * Get analog stick value
 *
 * @param stick Stick identifier (LX, LY, RX, RY)
 * @return Stick value (-1.0 to +1.0), or 0.0 if invalid/disconnected
 */
float controller_ps4_get_stick(ps4_analog_stick_t stick);

#endif // CONTROLLER_PS4_H
