/**
 * BTS7960 Motor Driver Controller Implementation
 */

#include "motor_bts7960.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <math.h>

static const char *TAG = "motor";

// Motor state
typedef struct {
    float target_speed;   // Target speed (-1.0 to +1.0)
    float current_speed;  // Current speed (ramped)
    bool inverted;        // Polarity inversion flag
} motor_state_t;

static motor_state_t motors[MOTOR_COUNT];
static bool motors_enabled = true;

// Helper: Clamp value to range
static inline float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Helper: Apply deadzone
static inline float apply_deadzone(float value, float deadzone) {
    if (fabsf(value) < deadzone) {
        return 0.0f;
    }
    return value;
}

// Helper: Set PWM duty cycle for a channel
static esp_err_t set_pwm_duty(ledc_channel_t channel, uint32_t duty) {
    esp_err_t err = ledc_set_duty(LEDC_SPEED_MODE, channel, duty);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(LEDC_SPEED_MODE, channel);
}

// Configure a single LEDC channel
static esp_err_t configure_pwm_channel(ledc_channel_t channel, gpio_num_t gpio) {
    ledc_channel_config_t channel_config = {
        .gpio_num   = gpio,
        .speed_mode = LEDC_SPEED_MODE,
        .channel    = channel,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER_NUM,
        .duty       = 0,
        .hpoint     = 0
    };
    return ledc_channel_config(&channel_config);
}

esp_err_t motor_init(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing motor driver (BTS7960)");
    ESP_LOGI(TAG, "PWM frequency: %d Hz, resolution: %d-bit",
             MOTOR_PWM_FREQUENCY_HZ, MOTOR_PWM_RESOLUTION);

    // Configure LEDC timer (shared by all PWM channels)
    ledc_timer_config_t timer_config = {
        .speed_mode      = LEDC_SPEED_MODE,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .timer_num       = LEDC_TIMER_NUM,
        .freq_hz         = MOTOR_PWM_FREQUENCY_HZ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure PWM channels for left motor
    ret = configure_pwm_channel(LEDC_CHANNEL_LEFT_FWD, PIN_MOTOR_LEFT_RPWM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure left motor RPWM: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = configure_pwm_channel(LEDC_CHANNEL_LEFT_REV, PIN_MOTOR_LEFT_LPWM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure left motor LPWM: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure PWM channels for right motor
    ret = configure_pwm_channel(LEDC_CHANNEL_RIGHT_FWD, PIN_MOTOR_RIGHT_RPWM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure right motor RPWM: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = configure_pwm_channel(LEDC_CHANNEL_RIGHT_REV, PIN_MOTOR_RIGHT_LPWM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure right motor LPWM: %s", esp_err_to_name(ret));
        return ret;
    }

#if MOTOR_USE_ENABLE_PINS
    // Configure enable pins as outputs (set HIGH to enable drivers)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_MOTOR_LEFT_R_EN) |
                        (1ULL << PIN_MOTOR_LEFT_L_EN) |
                        (1ULL << PIN_MOTOR_RIGHT_R_EN) |
                        (1ULL << PIN_MOTOR_RIGHT_L_EN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure enable pins: %s", esp_err_to_name(ret));
        return ret;
    }

    // Enable drivers by default
    gpio_set_level(PIN_MOTOR_LEFT_R_EN, 1);
    gpio_set_level(PIN_MOTOR_LEFT_L_EN, 1);
    gpio_set_level(PIN_MOTOR_RIGHT_R_EN, 1);
    gpio_set_level(PIN_MOTOR_RIGHT_L_EN, 1);
    ESP_LOGI(TAG, "Enable pins configured and set HIGH");
#else
    ESP_LOGI(TAG, "Enable pins not used (tied to 5V externally)");
#endif

    // Initialize motor state
    motors[MOTOR_LEFT].inverted = MOTOR_LEFT_INVERT;
    motors[MOTOR_RIGHT].inverted = MOTOR_RIGHT_INVERT;
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motors[i].target_speed = 0.0f;
        motors[i].current_speed = 0.0f;
    }

    ESP_LOGI(TAG, "Motor driver initialized successfully");
    return ESP_OK;
}

esp_err_t motor_set_speed(motor_id_t motor, float speed) {
    if (motor >= MOTOR_COUNT) {
        ESP_LOGE(TAG, "Invalid motor ID: %d", motor);
        return ESP_ERR_INVALID_ARG;
    }

    // Clamp to valid range
    speed = clamp(speed, -1.0f, 1.0f);

    // Apply inversion if configured
    if (motors[motor].inverted) {
        speed = -speed;
    }

    // Store target speed (actual speed will ramp in motor_update)
    motors[motor].target_speed = speed;

    return ESP_OK;
}

float motor_get_speed(motor_id_t motor) {
    if (motor >= MOTOR_COUNT) {
        return 0.0f;
    }
    float speed = motors[motor].current_speed;
    // Un-invert if needed (return speed in user's reference frame)
    if (motors[motor].inverted) {
        speed = -speed;
    }
    return speed;
}

esp_err_t motor_emergency_stop(void) {
    ESP_LOGW(TAG, "Emergency stop activated");

    // Immediately stop all motors (no ramping)
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motors[i].target_speed = 0.0f;
        motors[i].current_speed = 0.0f;
    }

    // Set all PWM outputs to 0
    set_pwm_duty(LEDC_CHANNEL_LEFT_FWD, 0);
    set_pwm_duty(LEDC_CHANNEL_LEFT_REV, 0);
    set_pwm_duty(LEDC_CHANNEL_RIGHT_FWD, 0);
    set_pwm_duty(LEDC_CHANNEL_RIGHT_REV, 0);

    return ESP_OK;
}

esp_err_t motor_enable(bool enable) {
#if MOTOR_USE_ENABLE_PINS
    motors_enabled = enable;
    int level = enable ? 1 : 0;
    gpio_set_level(PIN_MOTOR_LEFT_R_EN, level);
    gpio_set_level(PIN_MOTOR_LEFT_L_EN, level);
    gpio_set_level(PIN_MOTOR_RIGHT_R_EN, level);
    gpio_set_level(PIN_MOTOR_RIGHT_L_EN, level);
    ESP_LOGI(TAG, "Motors %s", enable ? "enabled" : "disabled");
#else
    ESP_LOGW(TAG, "Enable pins not controlled (tied externally)");
#endif
    return ESP_OK;
}

bool motor_is_enabled(void) {
    return motors_enabled;
}

esp_err_t motor_update(void) {
    for (int i = 0; i < MOTOR_COUNT; i++) {
        motor_state_t *m = &motors[i];

        // Apply ramping (rate limiting)
        float speed_diff = m->target_speed - m->current_speed;
        float max_change = MOTOR_RAMP_RATE * 0.01f; // Convert percentage to fraction

        if (fabsf(speed_diff) > max_change) {
            // Ramp towards target
            if (speed_diff > 0) {
                m->current_speed += max_change;
            } else {
                m->current_speed -= max_change;
            }
        } else {
            // Reached target
            m->current_speed = m->target_speed;
        }

        // Apply speed limits
        m->current_speed = clamp(m->current_speed, -MOTOR_MAX_DUTY, MOTOR_MAX_DUTY);

        // Calculate PWM duty cycle
        float speed = m->current_speed;
        uint32_t duty = (uint32_t)(fabsf(speed) * MOTOR_PWM_DUTY_MAX);

        // Determine direction and set appropriate PWM channels
        ledc_channel_t fwd_channel, rev_channel;
        if (i == MOTOR_LEFT) {
            fwd_channel = LEDC_CHANNEL_LEFT_FWD;
            rev_channel = LEDC_CHANNEL_LEFT_REV;
        } else {
            fwd_channel = LEDC_CHANNEL_RIGHT_FWD;
            rev_channel = LEDC_CHANNEL_RIGHT_REV;
        }

        if (speed > 0.0f) {
            // Forward
            set_pwm_duty(fwd_channel, duty);
            set_pwm_duty(rev_channel, 0);
        } else if (speed < 0.0f) {
            // Reverse
            set_pwm_duty(fwd_channel, 0);
            set_pwm_duty(rev_channel, duty);
        } else {
            // Stop (brake or coast depending on config)
#if MOTOR_BRAKE_MODE
            // Active brake (short circuit motor)
            set_pwm_duty(fwd_channel, MOTOR_PWM_DUTY_MAX);
            set_pwm_duty(rev_channel, MOTOR_PWM_DUTY_MAX);
#else
            // Coast (both sides off)
            set_pwm_duty(fwd_channel, 0);
            set_pwm_duty(rev_channel, 0);
#endif
        }
    }

    return ESP_OK;
}
