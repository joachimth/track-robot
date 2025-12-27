/**
 * Serial (UART) Control Module
 *
 * Accepts control commands via UART (GPIO20/21) in JSON format.
 * Provides a stable interface for external controllers (Raspberry Pi, Arduino, etc.)
 */

#ifndef CONTROLLER_SERIAL_H
#define CONTROLLER_SERIAL_H

#include "esp_err.h"

/**
 * Initialize serial control module
 *
 * Configures UART1 at 115200 baud and starts RX task.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t controller_serial_init(void);

/**
 * Serial control task (started by init)
 *
 * Runs continuously to receive and parse UART messages.
 * Automatically started by controller_serial_init().
 *
 * @param pvParameters Unused (FreeRTOS task parameter)
 */
void controller_serial_task(void *pvParameters);

/**
 * Send status message via serial (for debugging/monitoring)
 *
 * Sends current robot state as JSON to UART.
 * Only sent if SERIAL_SEND_STATUS is enabled in config.h.
 */
void controller_serial_send_status(void);

#endif // CONTROLLER_SERIAL_H
