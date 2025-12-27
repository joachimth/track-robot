/**
 * Serial (UART) Control Module Implementation
 */

#include "controller_serial.h"
#include "safety_failsafe.h"
#include "motor_bts7960.h"
#include "config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "serial";

// UART handle
static QueueHandle_t uart_queue = NULL;

esp_err_t controller_serial_init(void) {
#if ENABLE_SERIAL_CONTROL
    ESP_LOGI(TAG, "Initializing serial control");
    ESP_LOGI(TAG, "UART%d: TX=GPIO%d, RX=GPIO%d, Baud=%d",
             SERIAL_UART_NUM, SERIAL_TX_PIN, SERIAL_RX_PIN, SERIAL_BAUD_RATE);

    // Configure UART parameters
    uart_config_t uart_config = {
        .baud_rate = SERIAL_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(SERIAL_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set UART pins
    ret = uart_set_pin(SERIAL_UART_NUM,
                       SERIAL_TX_PIN,  // TX
                       SERIAL_RX_PIN,  // RX
                       UART_PIN_NO_CHANGE,  // RTS
                       UART_PIN_NO_CHANGE); // CTS
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Install UART driver
    ret = uart_driver_install(SERIAL_UART_NUM,
                              SERIAL_BUF_SIZE * 2,  // RX buffer
                              SERIAL_BUF_SIZE * 2,  // TX buffer
                              10,                    // Event queue size
                              &uart_queue,
                              0);                    // Interrupt flags
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create serial RX task
    BaseType_t task_ret = xTaskCreate(controller_serial_task,
                                      "serial_ctrl",
                                      TASK_STACK_SIZE_CONTROL,
                                      NULL,
                                      TASK_PRIORITY_CONTROL,
                                      NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create serial task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Serial control initialized successfully");
    return ESP_OK;
#else
    ESP_LOGI(TAG, "Serial control disabled in config");
    return ESP_OK;
#endif
}

void controller_serial_task(void *pvParameters) {
#if ENABLE_SERIAL_CONTROL
    uint8_t *data = (uint8_t *)malloc(SERIAL_BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RX buffer");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Serial RX task started");

#if SERIAL_CLI_MODE
    // Print CLI welcome message
    const char *welcome = "\r\nTrack Robot v" FIRMWARE_VERSION "\r\n"
                          "Type 'help' for commands\r\n> ";
    uart_write_bytes(SERIAL_UART_NUM, welcome, strlen(welcome));
#endif

    while (1) {
        // Read data from UART
        int len = uart_read_bytes(SERIAL_UART_NUM, data, SERIAL_BUF_SIZE - 1, pdMS_TO_TICKS(100));

        if (len > 0) {
            data[len] = '\0'; // Null-terminate

            // Remove trailing newline/CR
            while (len > 0 && (data[len-1] == '\n' || data[len-1] == '\r')) {
                data[--len] = '\0';
            }

            if (len == 0) continue; // Empty line

            ESP_LOGD(TAG, "RX: %s", data);

#if SERIAL_CLI_MODE
            // CLI mode: Parse text commands
            if (strcmp((char *)data, "help") == 0) {
                const char *help = "Commands:\r\n"
                                   "  fwd <speed>  - Move forward (0.0 to 1.0)\r\n"
                                   "  rev <speed>  - Move reverse (0.0 to 1.0)\r\n"
                                   "  left <speed> - Turn left (0.0 to 1.0)\r\n"
                                   "  right <speed>- Turn right (0.0 to 1.0)\r\n"
                                   "  stop         - Stop motors\r\n"
                                   "  estop        - Emergency stop\r\n"
                                   "  enable       - Clear e-stop\r\n"
                                   "  status       - Print current state\r\n"
                                   "> ";
                uart_write_bytes(SERIAL_UART_NUM, help, strlen(help));

            } else if (strcmp((char *)data, "stop") == 0) {
                safety_update_command(CONTROL_SOURCE_SERIAL, 0.0f, 0.0f, false, false);
                uart_write_bytes(SERIAL_UART_NUM, "Motors stopped\r\n> ", 17);

            } else if (strcmp((char *)data, "estop") == 0) {
                safety_trigger_estop();
                uart_write_bytes(SERIAL_UART_NUM, "E-STOP activated\r\n> ", 19);

            } else if (strcmp((char *)data, "enable") == 0) {
                safety_clear_estop();
                uart_write_bytes(SERIAL_UART_NUM, "E-STOP cleared\r\n> ", 17);

            } else if (strcmp((char *)data, "status") == 0) {
                controller_serial_send_status();
                uart_write_bytes(SERIAL_UART_NUM, "> ", 2);

            } else {
                // Try to parse as throttle/steering command
                // Format: "fwd 0.5", "left 1.0", etc.
                char cmd[16];
                float value;
                if (sscanf((char *)data, "%s %f", cmd, &value) == 2) {
                    if (strcmp(cmd, "fwd") == 0) {
                        safety_update_command(CONTROL_SOURCE_SERIAL, value, 0.0f, false, false);
                        uart_write_bytes(SERIAL_UART_NUM, "OK\r\n> ", 6);
                    } else if (strcmp(cmd, "rev") == 0) {
                        safety_update_command(CONTROL_SOURCE_SERIAL, -value, 0.0f, false, false);
                        uart_write_bytes(SERIAL_UART_NUM, "OK\r\n> ", 6);
                    } else if (strcmp(cmd, "left") == 0) {
                        safety_update_command(CONTROL_SOURCE_SERIAL, 0.0f, -value, false, false);
                        uart_write_bytes(SERIAL_UART_NUM, "OK\r\n> ", 6);
                    } else if (strcmp(cmd, "right") == 0) {
                        safety_update_command(CONTROL_SOURCE_SERIAL, 0.0f, value, false, false);
                        uart_write_bytes(SERIAL_UART_NUM, "OK\r\n> ", 6);
                    } else {
                        uart_write_bytes(SERIAL_UART_NUM, "Unknown command\r\n> ", 18);
                    }
                } else {
                    uart_write_bytes(SERIAL_UART_NUM, "Invalid syntax\r\n> ", 17);
                }
            }
#else
            // JSON mode: Parse JSON commands
            cJSON *json = cJSON_Parse((char *)data);
            if (json == NULL) {
                ESP_LOGW(TAG, "Invalid JSON: %s", data);
                const char *err = "{\"error\":\"json_parse_failed\"}\n";
                uart_write_bytes(SERIAL_UART_NUM, err, strlen(err));
                continue;
            }

            // Extract fields
            cJSON *throttle_obj = cJSON_GetObjectItem(json, "throttle");
            cJSON *steering_obj = cJSON_GetObjectItem(json, "steering");
            cJSON *estop_obj = cJSON_GetObjectItem(json, "estop");
            cJSON *slow_mode_obj = cJSON_GetObjectItem(json, "slow_mode");

            float throttle = cJSON_IsNumber(throttle_obj) ? (float)throttle_obj->valuedouble : 0.0f;
            float steering = cJSON_IsNumber(steering_obj) ? (float)steering_obj->valuedouble : 0.0f;
            bool estop = cJSON_IsBool(estop_obj) ? cJSON_IsTrue(estop_obj) : false;
            bool slow_mode = cJSON_IsBool(slow_mode_obj) ? cJSON_IsTrue(slow_mode_obj) : false;

            // Validate range
            if (throttle < -1.0f || throttle > 1.0f || steering < -1.0f || steering > 1.0f) {
                ESP_LOGW(TAG, "Value out of range: throttle=%.2f, steering=%.2f", throttle, steering);
                const char *err = "{\"error\":\"value_out_of_range\"}\n";
                uart_write_bytes(SERIAL_UART_NUM, err, strlen(err));
                cJSON_Delete(json);
                continue;
            }

            // Update safety subsystem
            safety_update_command(CONTROL_SOURCE_SERIAL, throttle, steering, estop, slow_mode);

            ESP_LOGD(TAG, "Command: throttle=%.2f, steering=%.2f, estop=%d, slow=%d",
                     throttle, steering, estop, slow_mode);

            cJSON_Delete(json);
#endif
        }

        // Periodic status update (if enabled)
#if SERIAL_SEND_STATUS && !SERIAL_CLI_MODE
        static uint32_t last_status_ms = 0;
        uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (now_ms - last_status_ms >= SERIAL_STATUS_RATE_MS) {
            controller_serial_send_status();
            last_status_ms = now_ms;
        }
#endif

        taskYIELD(); // Allow other tasks to run
    }

    free(data);
    vTaskDelete(NULL);
#endif
}

void controller_serial_send_status(void) {
#if ENABLE_SERIAL_CONTROL
    float throttle, steering;
    bool slow_mode;
    control_source_t source = safety_get_active_command(&throttle, &steering, &slow_mode);
    safety_status_t status = safety_get_status();

    const char *status_str = "ok";
    if (status == SAFETY_STATUS_ESTOP) {
        status_str = "estop";
    } else if (status == SAFETY_STATUS_FAILSAFE) {
        status_str = "failsafe";
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "{\"status\":\"%s\",\"source\":\"%s\",\"left\":%.2f,\"right\":%.2f}\n",
             status_str,
             safety_source_name(source),
             motor_get_speed(MOTOR_LEFT),
             motor_get_speed(MOTOR_RIGHT));

    uart_write_bytes(SERIAL_UART_NUM, msg, strlen(msg));
#endif
}
