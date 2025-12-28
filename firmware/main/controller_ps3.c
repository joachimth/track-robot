/**
 * PS3 Controller Input Implementation
 *
 * TODO: Implement PS3 controller support using ESP-IDF native Bluetooth Classic HID APIs.
 * The Arduino esp32-ps3 library is not compatible with ESP-IDF component system.
 *
 * For now, this is a stub implementation. Serial and HTTP controllers work.
 */

#include "controller_ps3.h"
#include "safety_failsafe.h"
#include "config.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include <string.h>

static const char *TAG = "ps3";

// Controller state
static bool connected = false;
static bool initialized = false;

esp_err_t controller_ps3_init(void) {
#if ENABLE_PS3_CONTROLLER
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  PS3 CONTROLLER - NOT YET IMPLEMENTED");
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "");
    ESP_LOGW(TAG, "The PS3 controller support requires");
    ESP_LOGW(TAG, "implementation using ESP-IDF's native");
    ESP_LOGW(TAG, "Bluetooth Classic HID stack.");
    ESP_LOGW(TAG, "");
    ESP_LOGW(TAG, "Alternative controllers available:");
    ESP_LOGW(TAG, "  - Serial (UART) - WORKING");
    ESP_LOGW(TAG, "  - HTTP (WiFi)   - WORKING");
    ESP_LOGW(TAG, "");
    ESP_LOGW(TAG, "Use 'idf.py menuconfig' to disable");
    ESP_LOGW(TAG, "PS3 controller if not needed.");
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "");

    // Print MAC address for future reference
    char mac_str[18];
    controller_ps3_get_mac(mac_str);
    ESP_LOGI(TAG, "Bluetooth MAC Address: %s", mac_str);

    initialized = true;
    return ESP_OK;
#else
    ESP_LOGI(TAG, "PS3 controller disabled in config");
    return ESP_OK;
#endif
}

esp_err_t controller_ps3_update(void) {
#if ENABLE_PS3_CONTROLLER
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Not connected (not implemented yet)
    if (!connected) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

bool controller_ps3_is_connected(void) {
#if ENABLE_PS3_CONTROLLER
    return connected;
#else
    return false;
#endif
}

esp_err_t controller_ps3_get_mac(char *mac_str) {
    if (mac_str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_BT);
    if (ret != ESP_OK) {
        // Fallback to WiFi MAC if BT MAC not available
        ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}
