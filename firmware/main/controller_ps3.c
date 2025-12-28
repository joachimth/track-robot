/**
 * PS3 Controller Input Implementation
 *
 * Uses jvpernis/esp32-ps3 library for Bluetooth Classic HID support.
 * Compatible with ESP32-S3 (Heltec WiFi Kit 32 V3).
 *
 * Installation: Clone PS3 library to components folder:
 *   git clone https://github.com/jvpernis/esp32-ps3.git components/ps3
 * Or for ESP-IDF v5 compatibility:
 *   git clone https://github.com/NSUHackspace/esp32-ps3-esp-idf-v5.git components/ps3
 */

#include "controller_ps3.h"
#include "safety_failsafe.h"
#include "config.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

// PS3 library header - will be available if cloned to components/ps3
#ifdef ENABLE_PS3_CONTROLLER
#if ENABLE_PS3_CONTROLLER
#include "ps3.h"
#endif
#endif

static const char *TAG = "ps3";

// Controller state
static bool connected = false;
static bool initialized = false;

// Previous button states for edge detection
static bool prev_estop_btn = false;
static bool prev_enable_btn = false;
static bool prev_slow_mode_btn = false;

/**
 * PS3 controller event callback
 *
 * Called by the PS3 library when controller state changes.
 */
static void ps3_event_callback(ps3_t ps3, ps3_event_t event) {
    // Update connection status
    connected = ps3IsConnected();

    if (!connected) {
        ESP_LOGW(TAG, "PS3 controller disconnected");
        return;
    }

    // Read analog stick values
    // Left stick Y: forward/backward (up = negative, down = positive)
    // Right stick X: steering (left = negative, right = positive)
    float throttle = -ps3.analog.stick.ly / 128.0f;  // Normalize to -1.0 to +1.0
    float steering = ps3.analog.stick.rx / 128.0f;   // Normalize to -1.0 to +1.0

    // Apply deadzone (configured in config.h)
    if (fabs(throttle) < PS3_DEADZONE) throttle = 0.0f;
    if (fabs(steering) < PS3_DEADZONE) steering = 0.0f;

    // Button mappings (from config.h defaults):
    // X button: Emergency stop
    // Start button: Clear e-stop / enable
    // Triangle button: Toggle slow mode
    bool estop_btn = ps3.button.cross;        // X button
    bool enable_btn = ps3.button.start;       // Start button
    bool slow_mode_btn = ps3.button.triangle; // Triangle button

    // Edge detection: trigger on button press (not hold)
    bool estop = estop_btn && !prev_estop_btn;
    bool enable = enable_btn && !prev_enable_btn;
    bool slow_mode_toggle = slow_mode_btn && !prev_slow_mode_btn;

    // Update previous states
    prev_estop_btn = estop_btn;
    prev_enable_btn = enable_btn;
    prev_slow_mode_btn = slow_mode_btn;

    // Handle e-stop button press
    if (estop) {
        ESP_LOGW(TAG, "E-STOP pressed via PS3 controller!");
        safety_trigger_estop();
    }

    // Handle enable button press
    if (enable) {
        ESP_LOGI(TAG, "Enable pressed via PS3 controller");
        safety_clear_estop();
    }

    // Static slow mode state (toggle on button press)
    static bool slow_mode = false;
    if (slow_mode_toggle) {
        slow_mode = !slow_mode;
        ESP_LOGI(TAG, "Slow mode: %s", slow_mode ? "ON" : "OFF");
    }

    // Update safety system with current command
    safety_update_command(CONTROL_SOURCE_PS3, throttle, steering, false, slow_mode);

    // Optional: Log significant stick movements (debug)
    if (fabs(throttle) > 0.1f || fabs(steering) > 0.1f) {
        ESP_LOGD(TAG, "PS3 input: throttle=%.2f, steering=%.2f", throttle, steering);
    }
}

esp_err_t controller_ps3_init(void) {
#if ENABLE_PS3_CONTROLLER
    ESP_LOGI(TAG, "Initializing PS3 controller (Bluetooth Classic)");

    // Print MAC address for pairing
    char mac_str[18];
    controller_ps3_get_mac(mac_str);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  PS3 CONTROLLER PAIRING INSTRUCTIONS");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Bluetooth MAC Address: %s", mac_str);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "Pairing steps:");
    ESP_LOGI(TAG, "1. Connect PS3 controller via USB to PC");
    ESP_LOGI(TAG, "2. Use SixaxisPairTool (Windows) or sixpair (Linux)");
    ESP_LOGI(TAG, "3. Set controller master to: %s", mac_str);
    ESP_LOGI(TAG, "4. Disconnect USB, press PS button");
    ESP_LOGI(TAG, "5. Controller should connect (LED 1 solid)");
    ESP_LOGI(TAG, "========================================");

    // Set event callback
    ps3SetEventCallback(ps3_event_callback);

    // Initialize PS3 library
    ps3Init();

    ESP_LOGI(TAG, "PS3 controller initialized, waiting for connection...");
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

    // Check connection status
    connected = ps3IsConnected();

    if (!connected) {
        // Not an error, just waiting for connection
        return ESP_ERR_NOT_FOUND;
    }

    // Connection active - events handled via callback
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
