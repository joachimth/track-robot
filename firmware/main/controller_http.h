/**
 * HTTP API Control Module
 *
 * Provides REST API endpoints for controlling the robot over Wi-Fi.
 * Supports both AP mode (default) and Station mode (connect to existing network).
 */

#ifndef CONTROLLER_HTTP_H
#define CONTROLLER_HTTP_H

#include "esp_err.h"

/**
 * Initialize HTTP control module
 *
 * Starts Wi-Fi (AP or STA mode) and HTTP server.
 *
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t controller_http_init(void);

/**
 * Stop HTTP server and Wi-Fi
 *
 * @return ESP_OK on success
 */
esp_err_t controller_http_stop(void);

#endif // CONTROLLER_HTTP_H
