/**
 * Track Robot Firmware - Main Entry Point
 *
 * ESP32-C5 tracked robot controller with multiple control interfaces:
 * - PS4 controller (Bluetooth LE) - Currently stubbed, see controller_ps4.c
 * - Serial (UART) - Fully functional
 * - HTTP API (Wi-Fi) - Fully functional
 *
 * Safety features:
 * - Emergency stop
 * - Failsafe timeout
 * - Control source arbitration
 * - Motor ramping
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "config.h"
#include "motor_bts7960.h"
#include "mixer_diffdrive.h"
#include "safety_failsafe.h"
#include "controller_ps4.h"
#include "controller_serial.h"
#include "controller_http.h"

static const char *TAG = "main";

// Status LED task
static void status_led_task(void *pvParameters) {
#if ENABLE_STATUS_LED
    // Configure status LED GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_STATUS_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    led_pattern_t current_pattern = LED_PATTERN_BOOT;
    uint32_t led_state = 0;
    uint32_t pattern_counter = 0;

    while (1) {
        // Determine LED pattern based on system state
        safety_status_t status = safety_get_status();
        bool ps4_connected = controller_ps4_is_connected();

        if (status == SAFETY_STATUS_ESTOP) {
            current_pattern = LED_PATTERN_ESTOP;
        } else if (status == SAFETY_STATUS_FAILSAFE) {
            current_pattern = LED_PATTERN_FAILSAFE;
        } else if (ps4_connected) {
            current_pattern = LED_PATTERN_CONNECTED;
        } else {
            current_pattern = LED_PATTERN_DISCONNECTED;
        }

        // Execute pattern
        switch (current_pattern) {
            case LED_PATTERN_BOOT:
                // Fast blink during boot
                led_state = (pattern_counter / (LED_BLINK_FAST_MS / 10)) % 2;
                break;

            case LED_PATTERN_DISCONNECTED:
                // Slow blink when no controller
                led_state = (pattern_counter / (LED_BLINK_SLOW_MS / 10)) % 2;
                break;

            case LED_PATTERN_CONNECTED:
                // Solid on when connected
                led_state = 1;
                break;

            case LED_PATTERN_ESTOP:
                // Rapid blink during e-stop
                led_state = (pattern_counter / (LED_BLINK_RAPID_MS / 10)) % 2;
                break;

            case LED_PATTERN_FAILSAFE:
                // Slow fade during failsafe (simplified as slow blink)
                led_state = (pattern_counter / (LED_BLINK_SLOW_MS / 10)) % 2;
                break;

            default:
                led_state = 0;
                break;
        }

        gpio_set_level(PIN_STATUS_LED, led_state);
        pattern_counter++;

        vTaskDelay(pdMS_TO_TICKS(10)); // 100Hz update rate
    }
#else
    vTaskDelete(NULL); // Delete this task if LED disabled
#endif
}

// Main control loop task
static void control_loop_task(void *pvParameters) {
    ESP_LOGI(TAG, "Control loop started (period: %d ms)", CONTROL_LOOP_PERIOD_MS);

    while (1) {
        // Get active control command from safety subsystem
        float throttle, steering;
        bool slow_mode;
        control_source_t source = safety_get_active_command(&throttle, &steering, &slow_mode);

        // Mix throttle/steering into left/right motor commands
        mixer_output_t mix = mixer_mix(throttle, steering, slow_mode);

        // Set motor speeds
        motor_set_speed(MOTOR_LEFT, mix.left);
        motor_set_speed(MOTOR_RIGHT, mix.right);

        // Update motor outputs (applies ramping)
        motor_update();

        // Update safety monitoring
        safety_update();

        // Update PS4 controller (if enabled)
#if ENABLE_PS4_CONTROLLER
        controller_ps4_update();
#endif

        // Log status periodically (every 1 second)
        static uint32_t log_counter = 0;
        if (log_counter++ >= (1000 / CONTROL_LOOP_PERIOD_MS)) {
            log_counter = 0;
            ESP_LOGI(TAG, "Source: %s, Throttle: %.2f, Steering: %.2f, Left: %.2f, Right: %.2f, Status: %s",
                     safety_source_name(source),
                     throttle, steering,
                     motor_get_speed(MOTOR_LEFT), motor_get_speed(MOTOR_RIGHT),
                     safety_is_estop_active() ? "E-STOP" :
                     (safety_get_status() == SAFETY_STATUS_FAILSAFE ? "FAILSAFE" : "OK"));
        }

        vTaskDelay(pdMS_TO_TICKS(CONTROL_LOOP_PERIOD_MS));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  Track Robot Firmware v%s", FIRMWARE_VERSION);
    ESP_LOGI(TAG, "  Build: %s %s", BUILD_DATE, BUILD_TIME);
    ESP_LOGI(TAG, "  ESP32-C5 @ %d MHz", CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    ESP_LOGI(TAG, "==================================================");

    // Initialize NVS (required for Wi-Fi and storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize subsystems
    ESP_LOGI(TAG, "Initializing subsystems...");

    ESP_LOGI(TAG, "[1/5] Safety & failsafe");
    safety_init();

    ESP_LOGI(TAG, "[2/5] Motor drivers");
    ESP_ERROR_CHECK(motor_init());

    ESP_LOGI(TAG, "[3/5] PS4 controller");
#if ENABLE_PS4_CONTROLLER
    ESP_ERROR_CHECK(controller_ps4_init());
#else
    ESP_LOGI(TAG, "  (disabled)");
#endif

    ESP_LOGI(TAG, "[4/5] Serial control");
#if ENABLE_SERIAL_CONTROL
    ESP_ERROR_CHECK(controller_serial_init());
#else
    ESP_LOGI(TAG, "  (disabled)");
#endif

    ESP_LOGI(TAG, "[5/5] HTTP control");
#if ENABLE_HTTP_CONTROL
    ESP_ERROR_CHECK(controller_http_init());
#else
    ESP_LOGI(TAG, "  (disabled)");
#endif

    ESP_LOGI(TAG, "All subsystems initialized successfully");

    // Create control loop task
    BaseType_t task_ret = xTaskCreate(control_loop_task,
                                      "control_loop",
                                      TASK_STACK_SIZE_MOTOR,
                                      NULL,
                                      TASK_PRIORITY_MOTOR,
                                      NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create control loop task");
        return;
    }

    // Create status LED task
    task_ret = xTaskCreate(status_led_task,
                           "status_led",
                           TASK_STACK_SIZE_LED,
                           NULL,
                           TASK_PRIORITY_LED,
                           NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status LED task");
        // Non-critical, continue anyway
    }

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  READY - Robot initialized successfully");
    ESP_LOGI(TAG, "  Control: Serial=%s, HTTP=%s, PS4=%s",
             ENABLE_SERIAL_CONTROL ? "ON" : "OFF",
             ENABLE_HTTP_CONTROL ? "ON" : "OFF",
             ENABLE_PS4_CONTROLLER ? "ON" : "OFF");
    ESP_LOGI(TAG, "==================================================");

    // Main task complete - FreeRTOS scheduler now runs
}
