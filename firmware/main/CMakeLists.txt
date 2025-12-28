# Track Robot Main Component

idf_component_register(
    SRCS
        "main.c"
        "motor_bts7960.c"
        "mixer_diffdrive.c"
        "safety_failsafe.c"
        "controller_ps3.c"
        "controller_serial.c"
        "controller_http.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver          # GPIO, LEDC, UART
        esp_timer       # Timing functions
        esp_wifi        # Wi-Fi for HTTP control
        esp_http_server # HTTP server
        esp_netif       # Network interface
        nvs_flash       # Non-volatile storage
        json            # cJSON library
        bt              # Bluetooth for PS3 controller
)
