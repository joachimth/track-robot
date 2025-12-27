/**
 * Track Robot Configuration
 *
 * Central configuration for all hardware and software parameters.
 * Edit this file to customize robot behavior.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"

// ============================================================================
// FIRMWARE VERSION
// ============================================================================
#define FIRMWARE_VERSION "1.0.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

// ============================================================================
// FEATURE ENABLES
// ============================================================================
#define ENABLE_PS4_CONTROLLER    1    // PS4 BLE controller support
#define ENABLE_SERIAL_CONTROL    1    // UART serial control
#define ENABLE_HTTP_CONTROL      1    // HTTP API control
#define ENABLE_STATUS_LED        1    // Status LED indicator

// ============================================================================
// GPIO PIN ASSIGNMENTS (ESP32-C5)
// ============================================================================

// Left Motor (BTS7960 #1)
#define PIN_MOTOR_LEFT_RPWM      GPIO_NUM_4    // PWM forward
#define PIN_MOTOR_LEFT_LPWM      GPIO_NUM_5    // PWM reverse
#define PIN_MOTOR_LEFT_R_EN      GPIO_NUM_6    // Enable (optional, can tie to 5V)
#define PIN_MOTOR_LEFT_L_EN      GPIO_NUM_7    // Enable (optional, can tie to 5V)

// Right Motor (BTS7960 #2)
#define PIN_MOTOR_RIGHT_RPWM     GPIO_NUM_8    // PWM forward
#define PIN_MOTOR_RIGHT_LPWM     GPIO_NUM_9    // PWM reverse
#define PIN_MOTOR_RIGHT_R_EN     GPIO_NUM_10   // Enable (optional, can tie to 5V)
#define PIN_MOTOR_RIGHT_L_EN     GPIO_NUM_11   // Enable (optional, can tie to 5V)

// Status LED
#define PIN_STATUS_LED           GPIO_NUM_18

// Serial Control (UART1)
#define PIN_SERIAL_TX            GPIO_NUM_20
#define PIN_SERIAL_RX            GPIO_NUM_21

// ============================================================================
// MOTOR CONTROL PARAMETERS
// ============================================================================

// PWM Configuration
#define MOTOR_PWM_FREQUENCY_HZ   20000         // 20kHz (above audible, within BTS7960 spec)
#define MOTOR_PWM_RESOLUTION     10            // 10-bit (1024 steps)
#define MOTOR_PWM_DUTY_MAX       1023          // Max duty cycle value (2^10 - 1)

// Motor Limits
#define MOTOR_MAX_DUTY           1.0f          // Max output (100%)
#define MOTOR_MIN_DUTY           0.0f          // Min output (0%, no deadband compensation)
#define MOTOR_RAMP_RATE          5.0f          // Max duty change per 10ms (5% per step)

// Motor Polarity (swap if wired backwards)
#define MOTOR_LEFT_INVERT        false         // Invert left motor direction
#define MOTOR_RIGHT_INVERT       false         // Invert right motor direction

// Enable Pin Control
#define MOTOR_USE_ENABLE_PINS    true          // Control EN pins via GPIO (vs tie to 5V)

// Braking Mode
#define MOTOR_BRAKE_MODE         true          // true = active brake, false = coast

// ============================================================================
// DIFFERENTIAL DRIVE MIXER
// ============================================================================

// Stick Deadzone (ignore inputs below this threshold)
#define STICK_DEADZONE           0.05f         // 5% deadzone

// Expo Curve (higher = less sensitive near center)
#define STICK_EXPO               1.5f          // 1.0 = linear, 2.0 = quadratic

// Speed Limits
#define THROTTLE_MAX             1.0f          // Max forward/reverse speed (100%)
#define STEERING_MAX             1.0f          // Max turn rate (100%)

// Slow Mode
#define SLOW_MODE_FACTOR         0.5f          // 50% speed limit when enabled

// ============================================================================
// SAFETY & FAILSAFE
// ============================================================================

// Failsafe Timeout
#define FAILSAFE_TIMEOUT_MS      500           // Stop motors if no command for 500ms

// E-Stop Behavior
#define ESTOP_LATCH              true          // E-stop latches until explicitly cleared

// Control Source Priority (0 = highest, 2 = lowest)
typedef enum {
    CONTROL_SOURCE_PS4,
    CONTROL_SOURCE_HTTP,
    CONTROL_SOURCE_SERIAL,
    CONTROL_SOURCE_NONE
} control_source_t;

#define CONTROL_SOURCE_PRIORITY_0  CONTROL_SOURCE_PS4     // Highest priority
#define CONTROL_SOURCE_PRIORITY_1  CONTROL_SOURCE_HTTP
#define CONTROL_SOURCE_PRIORITY_2  CONTROL_SOURCE_SERIAL  // Lowest priority

// ============================================================================
// PS4 CONTROLLER CONFIGURATION
// ============================================================================

#define PS4_DEVICE_NAME          "Track Robot"  // Bluetooth device name
#define PS4_RECONNECT_TIMEOUT_MS 10000          // Reconnect timeout (10 sec)

// Button Mapping
#define PS4_BTN_ESTOP            PS4_BUTTON_CROSS      // X button = emergency stop
#define PS4_BTN_ENABLE           PS4_BUTTON_START      // Start = enable after e-stop
#define PS4_BTN_SLOW_MODE        PS4_BUTTON_TRIANGLE   // Triangle = toggle slow mode

// Stick Mapping
#define PS4_STICK_THROTTLE       PS4_ANALOG_STICK_LY   // Left stick Y = forward/back
#define PS4_STICK_STEERING       PS4_ANALOG_STICK_RX   // Right stick X = turn

// ============================================================================
// SERIAL CONTROL CONFIGURATION
// ============================================================================

#define SERIAL_UART_NUM          UART_NUM_1
#define SERIAL_BAUD_RATE         115200
#define SERIAL_BUF_SIZE          256
#define SERIAL_CLI_MODE          false         // Enable CLI test mode
#define SERIAL_SEND_STATUS       true          // Send status messages back to host
#define SERIAL_STATUS_RATE_MS    100           // Status update interval

// ============================================================================
// HTTP CONTROL CONFIGURATION
// ============================================================================

#define HTTP_PORT                80
#define HTTP_MAX_CONNECTIONS     4             // Max concurrent clients
#define HTTP_SERVE_WEB_UI        true          // Serve web interface at /
#define HTTP_RATE_LIMIT          50            // Max requests/sec per endpoint
#define HTTP_CORS_ENABLED        true          // Enable CORS headers

// Wi-Fi Configuration (can also be set via menuconfig)
#ifndef CONFIG_WIFI_SSID
#define WIFI_SSID                "TrackRobot"      // Default AP SSID
#define WIFI_PASSWORD            "12345678"        // Default AP password (min 8 chars)
#define WIFI_AP_MODE             true              // true = AP mode, false = STA mode
#else
#define WIFI_SSID                CONFIG_WIFI_SSID
#define WIFI_PASSWORD            CONFIG_WIFI_PASSWORD
#define WIFI_AP_MODE             false             // Use STA mode if configured via menuconfig
#endif

#define WIFI_MAX_RETRY           5                 // Reconnect attempts in STA mode
#define WIFI_AP_MAX_CONN         4                 // Max clients in AP mode
#define WIFI_AP_CHANNEL          6                 // Wi-Fi channel in AP mode

// ============================================================================
// STATUS LED PATTERNS
// ============================================================================

typedef enum {
    LED_PATTERN_OFF,           // LED off (system halted)
    LED_PATTERN_BOOT,          // Fast blink (booting)
    LED_PATTERN_DISCONNECTED,  // Slow blink (no controller)
    LED_PATTERN_CONNECTED,     // Solid on (controller connected)
    LED_PATTERN_ESTOP,         // Rapid blink (emergency stop)
    LED_PATTERN_FAILSAFE       // Slow fade (failsafe active)
} led_pattern_t;

// LED Timing (milliseconds)
#define LED_BLINK_FAST_MS        100
#define LED_BLINK_SLOW_MS        500
#define LED_BLINK_RAPID_MS       50
#define LED_FADE_PERIOD_MS       2000

// ============================================================================
// LOGGING CONFIGURATION
// ============================================================================

// Log levels: NONE, ERROR, WARN, INFO, DEBUG, VERBOSE
#define LOG_LEVEL_MAIN           ESP_LOG_INFO
#define LOG_LEVEL_MOTOR          ESP_LOG_INFO
#define LOG_LEVEL_MIXER          ESP_LOG_INFO
#define LOG_LEVEL_SAFETY         ESP_LOG_INFO
#define LOG_LEVEL_PS4            ESP_LOG_INFO
#define LOG_LEVEL_SERIAL         ESP_LOG_INFO
#define LOG_LEVEL_HTTP           ESP_LOG_INFO

// ============================================================================
// TASK PRIORITIES & STACK SIZES
// ============================================================================

// Task priorities (higher = more priority, max = 25)
#define TASK_PRIORITY_SAFETY     20    // Highest (emergency stop, failsafe)
#define TASK_PRIORITY_MOTOR      15    // High (motor control)
#define TASK_PRIORITY_CONTROL    10    // Medium (input processing)
#define TASK_PRIORITY_HTTP        5    // Low (HTTP server)
#define TASK_PRIORITY_LED         1    // Lowest (status LED)

// Task stack sizes (bytes)
#define TASK_STACK_SIZE_SAFETY   2048
#define TASK_STACK_SIZE_MOTOR    3072
#define TASK_STACK_SIZE_CONTROL  4096
#define TASK_STACK_SIZE_HTTP     8192
#define TASK_STACK_SIZE_LED      1024

// ============================================================================
// ADVANCED TUNING (don't change unless you know what you're doing)
// ============================================================================

// Control Loop Timing
#define CONTROL_LOOP_PERIOD_MS   20    // 50Hz update rate

// LEDC Timer Configuration
#define LEDC_TIMER_NUM           LEDC_TIMER_0
#define LEDC_SPEED_MODE          LEDC_LOW_SPEED_MODE

// LEDC Channels (4 channels needed for 2 motors)
#define LEDC_CHANNEL_LEFT_FWD    LEDC_CHANNEL_0
#define LEDC_CHANNEL_LEFT_REV    LEDC_CHANNEL_1
#define LEDC_CHANNEL_RIGHT_FWD   LEDC_CHANNEL_2
#define LEDC_CHANNEL_RIGHT_REV   LEDC_CHANNEL_3

// NVS (Non-Volatile Storage) Namespace
#define NVS_NAMESPACE            "track_robot"

// Watchdog Timeout
#define WATCHDOG_TIMEOUT_MS      5000  // Reset if task hangs for >5 sec

#endif // CONFIG_H
