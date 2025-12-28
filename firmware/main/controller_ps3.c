/**
 * PS3 Controller Input Implementation
 *
 * Fully functional PS3 controller support using esp32-ps3 library.
 * Works with ESP32-S3 Bluetooth Classic.
 */

#include "controller_ps3.h"
#include "safety_failsafe.h"
#include "config.h"
#include "esp_log.h"
#include <Ps3Controller.h>
#include <string.h>

static const char *TAG = "ps3";

// Controller state
static bool connected = false;
static bool slow_mode = false;

// PS3 event callback
static void ps3_event_callback(void) {
    // Connection status
    if (Ps3.isConnected()) {
        if (!connected) {
            ESP_LOGI(TAG, "PS3 controller connected!");
            connected = true;
        }
    } else {
        if (connected) {
            ESP_LOGW(TAG, "PS3 controller disconnected");
            connected = false;
        }
        return;
    }

    // Read analog sticks
    // PS3 left stick Y: 0-255 (128 = center)
    // PS3 right stick X: 0-255 (128 = center)
    int left_y = Ps3.data.analog.stick.ly;   // Forward/back
    int right_x = Ps3.data.analog.stick.rx;  // Steering

    // Convert from 0-255 to -1.0 to +1.0
    float throttle = -(left_y - 128) / 128.0f;  // Invert Y axis
    float steering = (right_x - 128) / 128.0f;

    // Read buttons
    bool estop_btn = Ps3.data.button.cross;      // X button
    bool enable_btn = Ps3.data.button.start;     // Start button
    bool slow_btn = Ps3.data.button.triangle;    // Triangle button

    // Toggle slow mode on button press (with debounce)
    static bool slow_btn_prev = false;
    if (slow_btn && !slow_btn_prev) {
        slow_mode = !slow_mode;
        ESP_LOGI(TAG, "Slow mode %s", slow_mode ? "ON" : "OFF");

        // Vibrate controller as feedback
        Ps3.setRumble(100.0, 200);  // Small rumble for 200ms
    }
    slow_btn_prev = slow_btn;

    // Clear e-stop on Start button
    if (enable_btn) {
        safety_clear_estop();
        ESP_LOGI(TAG, "E-stop cleared via Start button");
    }

    // Update safety subsystem with current command
    safety_update_command(CONTROL_SOURCE_PS3, throttle, steering, estop_btn, slow_mode);

    // Set LED color based on status
    if (safety_is_estop_active()) {
        Ps3.setLed(255, 0, 0);  // Red = E-stop
    } else if (slow_mode) {
        Ps3.setLed(255, 255, 0);  // Yellow = Slow mode
    } else {
        Ps3.setLed(0, 0, 255);  // Blue = Normal
    }
}

// Connection callback
static void ps3_connect_callback(void) {
    ESP_LOGI(TAG, "PS3 controller connected!");
    connected = true;

    // Set initial LED color (blue)
    Ps3.setLed(0, 0, 255);
}

// Disconnection callback
static void ps3_disconnect_callback(void) {
    ESP_LOGW(TAG, "PS3 controller disconnected");
    connected = false;
    slow_mode = false;

    // Send zero command to safety subsystem
    safety_update_command(CONTROL_SOURCE_PS3, 0.0f, 0.0f, false, false);
}

esp_err_t controller_ps3_init(void) {
#if ENABLE_PS3_CONTROLLER
    ESP_LOGI(TAG, "Initializing PS3 controller (Bluetooth Classic)");
    ESP_LOGI(TAG, "Device name: %s", PS3_DEVICE_NAME);

    // Register callbacks
    Ps3.attach(ps3_event_callback);
    Ps3.attachOnConnect(ps3_connect_callback);
    Ps3.attachOnDisconnect(ps3_disconnect_callback);

    // Start listening for PS3 controller
    if (!Ps3.begin(PS3_DEVICE_NAME)) {
        ESP_LOGE(TAG, "Failed to initialize PS3 controller");
        return ESP_FAIL;
    }

    // Print MAC address for pairing
    char mac_str[18];
    controller_ps3_get_mac(mac_str);
    ESP_LOGI(TAG, "ESP32 MAC Address: %s", mac_str);
    ESP_LOGI(TAG, "Waiting for PS3 controller to connect...");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  PS3 PAIRING INSTRUCTIONS:");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "1. Install SixaxisPairTool on your PC");
    ESP_LOGI(TAG, "   Download: https://www.gimx.fr/wiki/index.php?title=Sixaxis_Pair_Tool");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "2. Connect PS3 controller to PC via USB");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "3. Open SixaxisPairTool and change the");
    ESP_LOGI(TAG, "   'Master' address to: %s", mac_str);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "4. Click 'Update' in SixaxisPairTool");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "5. Disconnect USB and press PS button");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "6. Controller should connect automatically");
    ESP_LOGI(TAG, "   (LEDs will stop blinking when connected)");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    ESP_LOGI(TAG, "PS3 controller module initialized successfully");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "PS3 controller disabled in config");
    return ESP_OK;
#endif
}

esp_err_t controller_ps3_update(void) {
#if ENABLE_PS3_CONTROLLER
    // Event callback handles updates, this is just for status checks
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
    esp_read_mac(mac, ESP_MAC_BT);
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}
