/**
 * Safety and Failsafe Controller Implementation
 */

#include "safety_failsafe.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "safety";

// Control command storage (one per source)
static control_command_t commands[4]; // PS4, HTTP, Serial, None

// Safety state
static bool estop_latched = false;
static safety_status_t current_status = SAFETY_STATUS_OK;

// Source priority order (configurable in config.h)
static const control_source_t source_priority[3] = {
    CONTROL_SOURCE_PRIORITY_0,
    CONTROL_SOURCE_PRIORITY_1,
    CONTROL_SOURCE_PRIORITY_2
};

// Helper: Get current time in milliseconds
static inline uint32_t millis(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void safety_init(void) {
    ESP_LOGI(TAG, "Initializing safety subsystem");
    ESP_LOGI(TAG, "Failsafe timeout: %d ms", FAILSAFE_TIMEOUT_MS);
    ESP_LOGI(TAG, "E-stop latch: %s", ESTOP_LATCH ? "enabled" : "disabled");
    ESP_LOGI(TAG, "Control priority: %s > %s > %s",
             safety_source_name(CONTROL_SOURCE_PRIORITY_0),
             safety_source_name(CONTROL_SOURCE_PRIORITY_1),
             safety_source_name(CONTROL_SOURCE_PRIORITY_2));

    // Initialize all commands to safe state
    for (int i = 0; i < 4; i++) {
        commands[i].throttle = 0.0f;
        commands[i].steering = 0.0f;
        commands[i].estop = false;
        commands[i].slow_mode = false;
        commands[i].timestamp = 0;
        commands[i].source = (control_source_t)i;
    }

    estop_latched = false;
    current_status = SAFETY_STATUS_OK;
}

void safety_update_command(control_source_t source, float throttle,
                           float steering, bool estop, bool slow_mode) {
    if (source >= CONTROL_SOURCE_NONE) {
        ESP_LOGW(TAG, "Invalid control source: %d", source);
        return;
    }

    control_command_t *cmd = &commands[source];
    cmd->throttle = throttle;
    cmd->steering = steering;
    cmd->slow_mode = slow_mode;
    cmd->timestamp = millis();
    cmd->source = source;

    // Check for e-stop trigger
    if (estop && !cmd->estop) {
        ESP_LOGW(TAG, "E-stop triggered by %s", safety_source_name(source));
        safety_trigger_estop();
    }
    cmd->estop = estop;
}

control_source_t safety_get_active_command(float *throttle, float *steering,
                                            bool *slow_mode) {
    uint32_t now = millis();
    control_source_t active_source = CONTROL_SOURCE_NONE;

    // Check if e-stop is active
    if (estop_latched) {
        *throttle = 0.0f;
        *steering = 0.0f;
        *slow_mode = false;
        return CONTROL_SOURCE_NONE;
    }

    // Find highest-priority source with recent command
    for (int i = 0; i < 3; i++) {
        control_source_t src = source_priority[i];
        control_command_t *cmd = &commands[src];

        uint32_t age = now - cmd->timestamp;
        if (age < FAILSAFE_TIMEOUT_MS) {
            // This source is active
            *throttle = cmd->throttle;
            *steering = cmd->steering;
            *slow_mode = cmd->slow_mode;
            active_source = src;

            // Update status to OK if we have an active source
            if (current_status == SAFETY_STATUS_FAILSAFE) {
                ESP_LOGI(TAG, "Failsafe cleared, active source: %s",
                         safety_source_name(src));
                current_status = SAFETY_STATUS_OK;
            }
            return src;
        }
    }

    // No active source found (failsafe)
    if (current_status != SAFETY_STATUS_FAILSAFE) {
        ESP_LOGW(TAG, "Failsafe triggered - no active control source");
        current_status = SAFETY_STATUS_FAILSAFE;
    }

    *throttle = 0.0f;
    *steering = 0.0f;
    *slow_mode = false;
    return CONTROL_SOURCE_NONE;
}

safety_status_t safety_get_status(void) {
    if (estop_latched) {
        return SAFETY_STATUS_ESTOP;
    }
    return current_status;
}

void safety_trigger_estop(void) {
    if (!estop_latched) {
        ESP_LOGW(TAG, "Emergency stop activated");
    }
    estop_latched = true;
    current_status = SAFETY_STATUS_ESTOP;
}

void safety_clear_estop(void) {
    if (estop_latched) {
        ESP_LOGI(TAG, "Emergency stop cleared");
    }
    estop_latched = false;

    // Revert to OK or failsafe depending on active sources
    float throttle, steering;
    bool slow_mode;
    control_source_t src = safety_get_active_command(&throttle, &steering, &slow_mode);
    if (src == CONTROL_SOURCE_NONE) {
        current_status = SAFETY_STATUS_FAILSAFE;
    } else {
        current_status = SAFETY_STATUS_OK;
    }
}

bool safety_is_estop_active(void) {
    return estop_latched;
}

void safety_update(void) {
    // Periodic safety check (called from main loop)
    // Most logic is in safety_get_active_command(), which is called
    // more frequently. This function can be used for additional
    // periodic checks if needed in the future.

    // Future: Add watchdog check, voltage monitoring, etc.
}

const char *safety_source_name(control_source_t source) {
    switch (source) {
        case CONTROL_SOURCE_PS3:    return "PS3";
        case CONTROL_SOURCE_HTTP:   return "HTTP";
        case CONTROL_SOURCE_SERIAL: return "Serial";
        case CONTROL_SOURCE_NONE:   return "None";
        default:                    return "Unknown";
    }
}
