/**
 * HTTP API Control Module Implementation
 */

#include "controller_http.h"
#include "safety_failsafe.h"
#include "motor_bts7960.h"
#include "config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "http";

static httpd_handle_t server = NULL;
static bool wifi_connected = false;

// Forward declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data);
static esp_err_t start_webserver(void);
static void stop_webserver(void);

// HTTP handler functions
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t control_handler(httpd_req_t *req);
static esp_err_t estop_handler(httpd_req_t *req);
static esp_err_t enable_handler(httpd_req_t *req);
static esp_err_t status_handler(httpd_req_t *req);

// Helper: Set CORS headers
static void set_cors_headers(httpd_req_t *req) {
#if HTTP_CORS_ENABLED
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
#endif
}

esp_err_t controller_http_init(void) {
#if ENABLE_HTTP_CONTROL
    ESP_LOGI(TAG, "Initializing HTTP control module");

    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#if WIFI_AP_MODE
    // Access Point mode
    ESP_LOGI(TAG, "Starting Wi-Fi in AP mode");
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK
        },
    };

    if (strlen(WIFI_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi AP started: SSID=%s, Channel=%d", WIFI_SSID, WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "Connect to http://192.168.4.1:%d/", HTTP_PORT);

    wifi_connected = true;
    start_webserver();

#else
    // Station mode
    ESP_LOGI(TAG, "Starting Wi-Fi in STA mode");
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", WIFI_SSID);
#endif

    return ESP_OK;
#else
    ESP_LOGI(TAG, "HTTP control disabled in config");
    return ESP_OK;
#endif
}

esp_err_t controller_http_stop(void) {
#if ENABLE_HTTP_CONTROL
    stop_webserver();
    esp_wifi_stop();
    wifi_connected = false;
    ESP_LOGI(TAG, "HTTP control stopped");
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_AP_STACONNECTED) {
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Client connected: " MACSTR, MAC2STR(event->mac));
        } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
            wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Client disconnected: " MACSTR, MAC2STR(event->mac));
        } else if (event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Disconnected from AP, retrying...");
            esp_wifi_connect();
            wifi_connected = false;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connect to http://" IPSTR ":%d/", IP2STR(&event->ip_info.ip), HTTP_PORT);
        wifi_connected = true;
        start_webserver();
    }
}

static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_PORT;
    config.max_open_sockets = HTTP_MAX_CONNECTIONS;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP server on port %d", HTTP_PORT);

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    // Register URI handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &root_uri);

    httpd_uri_t control_uri = {
        .uri = "/api/control",
        .method = HTTP_POST,
        .handler = control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &control_uri);

    httpd_uri_t estop_uri = {
        .uri = "/api/estop",
        .method = HTTP_POST,
        .handler = estop_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &estop_uri);

    httpd_uri_t enable_uri = {
        .uri = "/api/enable",
        .method = HTTP_POST,
        .handler = enable_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &enable_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

static void stop_webserver(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}

// Handler: Root (/) - Serve minimal web UI
static esp_err_t root_handler(httpd_req_t *req) {
    set_cors_headers(req);

#if HTTP_SERVE_WEB_UI
    const char *html = "<!DOCTYPE html><html><head><title>Track Robot</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<style>body{font-family:Arial;text-align:center;padding:20px;}"
        "button{font-size:18px;padding:15px 30px;margin:10px;}"
        ".joystick{width:200px;height:200px;background:#ddd;border-radius:50%;margin:20px auto;position:relative;}"
        ".status{background:#f0f0f0;padding:10px;margin:10px;border-radius:5px;}"
        "</style></head><body>"
        "<h1>Track Robot Control</h1>"
        "<div class='status' id='status'>Status: Connecting...</div>"
        "<div class='joystick' id='joystick'></div>"
        "<button onclick='sendCommand(0.5,0)'>Forward</button>"
        "<button onclick='sendCommand(-0.5,0)'>Reverse</button><br>"
        "<button onclick='sendCommand(0,-0.5)'>Left</button>"
        "<button onclick='sendCommand(0,0.5)'>Right</button><br>"
        "<button onclick='sendCommand(0,0)'>Stop</button><br>"
        "<button onclick='estop()' style='background:red;color:white;'>E-STOP</button>"
        "<button onclick='enable()'>Enable</button>"
        "<script>"
        "function sendCommand(t,s){fetch('/api/control',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({throttle:t,steering:s})}).then(r=>r.json()).then(d=>updateStatus(d));}"
        "function estop(){fetch('/api/estop',{method:'POST'}).then(r=>r.json()).then(d=>updateStatus(d));}"
        "function enable(){fetch('/api/enable',{method:'POST'}).then(r=>r.json()).then(d=>updateStatus(d));}"
        "function updateStatus(d){document.getElementById('status').innerText='Status: '+d.status+' | Source: '+(d.source||'none');}"
        "setInterval(()=>fetch('/api/status').then(r=>r.json()).then(d=>updateStatus(d)),1000);"
        "</script></body></html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
#else
    const char *msg = "{\"message\":\"Web UI disabled, use /api endpoints\"}\n";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, msg, HTTPD_RESP_USE_STRLEN);
#endif
}

// Handler: POST /api/control
static esp_err_t control_handler(httpd_req_t *req) {
    set_cors_headers(req);

    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        const char *err = "{\"error\":\"invalid_json\"}\n";
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, err, HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    cJSON *throttle_obj = cJSON_GetObjectItem(json, "throttle");
    cJSON *steering_obj = cJSON_GetObjectItem(json, "steering");
    cJSON *estop_obj = cJSON_GetObjectItem(json, "estop");
    cJSON *slow_obj = cJSON_GetObjectItem(json, "slow_mode");

    float throttle = cJSON_IsNumber(throttle_obj) ? (float)throttle_obj->valuedouble : 0.0f;
    float steering = cJSON_IsNumber(steering_obj) ? (float)steering_obj->valuedouble : 0.0f;
    bool estop = cJSON_IsBool(estop_obj) ? cJSON_IsTrue(estop_obj) : false;
    bool slow_mode = cJSON_IsBool(slow_obj) ? cJSON_IsTrue(slow_obj) : false;

    cJSON_Delete(json);

    // Update safety
    safety_update_command(CONTROL_SOURCE_HTTP, throttle, steering, estop, slow_mode);

    // Send response
    char resp[128];
    snprintf(resp, sizeof(resp),
             "{\"status\":\"ok\",\"throttle\":%.2f,\"steering\":%.2f,\"source\":\"http\"}\n",
             throttle, steering);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// Handler: POST /api/estop
static esp_err_t estop_handler(httpd_req_t *req) {
    set_cors_headers(req);
    safety_trigger_estop();

    const char *resp = "{\"status\":\"estop\",\"message\":\"Emergency stop activated\"}\n";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// Handler: POST /api/enable
static esp_err_t enable_handler(httpd_req_t *req) {
    set_cors_headers(req);
    safety_clear_estop();

    const char *resp = "{\"status\":\"ok\",\"message\":\"Motors enabled\"}\n";
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

// Handler: GET /api/status
static esp_err_t status_handler(httpd_req_t *req) {
    set_cors_headers(req);

    float throttle, steering;
    bool slow_mode;
    control_source_t source = safety_get_active_command(&throttle, &steering, &slow_mode);
    safety_status_t status = safety_get_status();

    const char *status_str = (status == SAFETY_STATUS_ESTOP) ? "estop" :
                             (status == SAFETY_STATUS_FAILSAFE) ? "failsafe" : "ok";

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"status\":\"%s\",\"control_source\":\"%s\","
             "\"throttle\":%.2f,\"steering\":%.2f,"
             "\"motor_left\":%.2f,\"motor_right\":%.2f,"
             "\"estop\":%s,\"slow_mode\":%s,"
             "\"uptime_sec\":%u,\"free_heap\":%u}\n",
             status_str, safety_source_name(source),
             throttle, steering,
             motor_get_speed(MOTOR_LEFT), motor_get_speed(MOTOR_RIGHT),
             safety_is_estop_active() ? "true" : "false",
             slow_mode ? "true" : "false",
             (unsigned)(esp_timer_get_time() / 1000000),
             (unsigned)esp_get_free_heap_size());

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}
