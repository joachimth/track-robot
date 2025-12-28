/**
 * PS3 Controller Input (Bluetooth Classic)
 *
 * WORKS with ESP32-S3! ESP32-S3 supports Bluetooth Classic for PS3 controllers.
 * Uses the esp32-ps3 library by jvpernis.
 */

#ifndef CONTROLLER_PS3_H
#define CONTROLLER_PS3_H

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize PS3 controller module
 *
 * Initializes Bluetooth Classic stack and starts listening for PS3 controller.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t controller_ps3_init(void);

/**
 * Update PS3 controller state (call periodically)
 *
 * Reads latest controller input and updates safety subsystem.
 *
 * @return ESP_OK if controller connected, ESP_ERR_NOT_FOUND if disconnected
 */
esp_err_t controller_ps3_update(void);

/**
 * Check if PS3 controller is connected
 *
 * @return true if connected, false otherwise
 */
bool controller_ps3_is_connected(void);

/**
 * Get PS3 controller MAC address for pairing
 *
 * Print this MAC address and use it to pair your PS3 controller.
 *
 * @param mac_str Buffer to store MAC address string (min 18 bytes)
 * @return ESP_OK on success
 */
esp_err_t controller_ps3_get_mac(char *mac_str);

#endif // CONTROLLER_PS3_H
