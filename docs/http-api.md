# HTTP API Reference

## Overview

The HTTP API allows control of the robot over Wi-Fi using standard REST endpoints. This enables:
- Web-based control interface (browser joystick)
- Integration with home automation (Home Assistant, Node-RED, etc.)
- Remote control from PC/phone without Bluetooth
- Telemetry and status monitoring

**Base URL:** `http://<ESP32_IP_ADDRESS>`

**Find ESP32 IP:** Check serial console during boot, or use router DHCP table.

## Authentication & Security

⚠️ **WARNING: No authentication is implemented by default.**

This API is designed for **local network use only**:
- Do NOT expose to the internet (no TLS, no auth)
- Use on trusted networks only (home/lab Wi-Fi)
- Consider adding firewall rules to restrict access

**Future enhancement:** Add API key or HTTP Basic Auth (see extension notes).

## Endpoints

### 1. Control Robot

**POST** `/api/control`

Set throttle and steering values.

**Request:**
```http
POST /api/control HTTP/1.1
Host: 192.168.1.100
Content-Type: application/json

{
  "throttle": 0.5,
  "steering": 0.0
}
```

**Request Body:**
| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `throttle` | float | -1.0 to +1.0 | Forward/reverse (+ = forward) |
| `steering` | float | -1.0 to +1.0 | Left/right turn (+ = right) |
| `estop` | bool | optional | Emergency stop (true = stop) |
| `slow_mode` | bool | optional | Enable slow mode (true = 50% limit) |

**Response (200 OK):**
```json
{
  "status": "ok",
  "throttle": 0.5,
  "steering": 0.0,
  "source": "http"
}
```

**Response Fields:**
| Field | Type | Description |
|-------|------|-------------|
| `status` | string | "ok", "estop", "failsafe" |
| `throttle` | float | Applied throttle value |
| `steering` | float | Applied steering value |
| `source` | string | Active control source ("ps4", "http", "serial") |

**Error Response (400 Bad Request):**
```json
{
  "error": "invalid_json"
}
```

**Error Response (429 Too Many Requests):**
```json
{
  "error": "rate_limit_exceeded",
  "retry_after": 0.1
}
```

**Example (curl):**
```bash
curl -X POST http://192.168.1.100/api/control \
  -H "Content-Type: application/json" \
  -d '{"throttle":0.5,"steering":0.0}'
```

**Example (Python):**
```python
import requests
response = requests.post('http://192.168.1.100/api/control', json={
    'throttle': 0.5,
    'steering': 0.0
})
print(response.json())
```

**Example (JavaScript):**
```javascript
fetch('http://192.168.1.100/api/control', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({throttle: 0.5, steering: 0.0})
})
.then(res => res.json())
.then(data => console.log(data));
```

---

### 2. Emergency Stop

**POST** `/api/estop`

Trigger emergency stop (latched until cleared).

**Request:**
```http
POST /api/estop HTTP/1.1
Host: 192.168.1.100
```

**Response (200 OK):**
```json
{
  "status": "estop",
  "message": "Emergency stop activated"
}
```

**Example:**
```bash
curl -X POST http://192.168.1.100/api/estop
```

---

### 3. Enable (Clear E-Stop)

**POST** `/api/enable`

Clear emergency stop and allow motor control.

**Request:**
```http
POST /api/enable HTTP/1.1
Host: 192.168.1.100
```

**Response (200 OK):**
```json
{
  "status": "ok",
  "message": "Motors enabled"
}
```

**Example:**
```bash
curl -X POST http://192.168.1.100/api/enable
```

---

### 4. Get Status

**GET** `/api/status`

Query current robot state.

**Request:**
```http
GET /api/status HTTP/1.1
Host: 192.168.1.100
```

**Response (200 OK):**
```json
{
  "status": "ok",
  "control_source": "http",
  "throttle": 0.5,
  "steering": 0.0,
  "motor_left": 0.5,
  "motor_right": 0.5,
  "estop": false,
  "slow_mode": false,
  "uptime_sec": 3600,
  "free_heap": 245760,
  "wifi_rssi": -45
}
```

**Response Fields:**
| Field | Type | Description |
|-------|------|-------------|
| `status` | string | "ok", "estop", "failsafe" |
| `control_source` | string | "ps4", "http", "serial", "none" |
| `throttle` | float | Current throttle command (-1.0 to +1.0) |
| `steering` | float | Current steering command (-1.0 to +1.0) |
| `motor_left` | float | Left motor output (-1.0 to +1.0) |
| `motor_right` | float | Right motor output (-1.0 to +1.0) |
| `estop` | bool | Emergency stop state |
| `slow_mode` | bool | Slow mode enabled |
| `uptime_sec` | int | Seconds since boot |
| `free_heap` | int | Free RAM (bytes) |
| `wifi_rssi` | int | Wi-Fi signal strength (dBm) |

**Example:**
```bash
curl http://192.168.1.100/api/status | jq
```

---

### 5. Root (Web Interface)

**GET** `/`

Serve minimal web control interface (optional).

**Response (200 OK):**
Returns HTML page with joystick controls (see Web UI section below).

**Example:**
Open `http://192.168.1.100/` in web browser.

---

## Control Arbitration

HTTP control integrates with multi-source control system:

**Default priority:** PS4 > **HTTP** > Serial

- If PS4 controller is active, HTTP commands are ignored
- If PS4 disconnects (timeout), HTTP can take control
- HTTP commands timeout after 500ms (failsafe)

**To change priority**, edit `firmware/main/config.h`:
```c
#define CONTROL_SOURCE_PRIORITY_0  CONTROL_SOURCE_HTTP   // Highest
#define CONTROL_SOURCE_PRIORITY_1  CONTROL_SOURCE_SERIAL
#define CONTROL_SOURCE_PRIORITY_2  CONTROL_SOURCE_PS4    // Lowest
```

## Rate Limiting

To prevent abuse and ensure responsiveness:
- **Max request rate:** 50 requests/sec (per endpoint)
- **Exceeding limit:** Returns HTTP 429 (Too Many Requests)
- **Recommended rate:** 10-20 requests/sec (plenty for smooth control)

## Web UI (Minimal Example)

A basic web interface is included at the root path (`/`):

**Features:**
- Virtual joystick (touch/mouse)
- Emergency stop button
- Status display
- Works on mobile and desktop

**Screenshot:**
```
┌─────────────────────────────────────┐
│  Track Robot Control                │
│                                     │
│   Status: OK  |  Source: HTTP       │
│   Battery: 12.3V  |  RSSI: -45dBm  │
│                                     │
│         ┌───────┐                   │
│         │   ↑   │  Throttle         │
│         │ ←   → │  Steering         │
│         │   ↓   │                   │
│         └───────┘                   │
│                                     │
│   [EMERGENCY STOP]  [Enable]       │
│                                     │
└─────────────────────────────────────┘
```

**To disable web UI**, edit `config.h`:
```c
#define HTTP_SERVE_WEB_UI  false  // Only serve API endpoints
```

## Usage Examples

### Example 1: Simple Drive Script (Python)

```python
#!/usr/bin/env python3
import requests
import time

BASE_URL = 'http://192.168.1.100'

def drive(throttle, steering, duration=1.0):
    """Send drive command and hold for duration"""
    start = time.time()
    while time.time() - start < duration:
        response = requests.post(f'{BASE_URL}/api/control', json={
            'throttle': throttle,
            'steering': steering
        })
        if response.status_code == 200:
            print(f"Moving: throttle={throttle}, steering={steering}")
        time.sleep(0.05)  # 20Hz update rate

def stop():
    """Stop robot"""
    requests.post(f'{BASE_URL}/api/control', json={
        'throttle': 0.0,
        'steering': 0.0
    })
    print("Stopped")

# Test sequence
try:
    drive(0.5, 0.0, duration=2.0)   # Forward 2 sec
    drive(0.0, 1.0, duration=1.0)   # Turn right 1 sec
    drive(0.5, 0.0, duration=2.0)   # Forward 2 sec
    drive(0.0, -1.0, duration=1.0)  # Turn left 1 sec
    stop()
except KeyboardInterrupt:
    requests.post(f'{BASE_URL}/api/estop')  # E-stop on Ctrl+C
    print("Emergency stop!")
```

### Example 2: Gamepad Control (Python + pygame)

```python
#!/usr/bin/env python3
import requests
import pygame
import time

BASE_URL = 'http://192.168.1.100'

pygame.init()
pygame.joystick.init()
joystick = pygame.joystick.Joystick(0)
joystick.init()

print(f"Using joystick: {joystick.get_name()}")

try:
    while True:
        pygame.event.pump()

        throttle = -joystick.get_axis(1)  # Left stick Y (inverted)
        steering = joystick.get_axis(2)   # Right stick X

        # Emergency stop on button press
        if joystick.get_button(0):  # A button
            requests.post(f'{BASE_URL}/api/estop')
            print("E-STOP!")
            time.sleep(0.5)
            continue

        # Send command
        response = requests.post(f'{BASE_URL}/api/control', json={
            'throttle': throttle,
            'steering': steering
        }, timeout=0.1)

        if response.status_code == 200:
            data = response.json()
            print(f"Source: {data['source']}, L: {data['throttle']:.2f}, R: {data['steering']:.2f}", end='\r')

        time.sleep(0.05)  # 20Hz

except KeyboardInterrupt:
    requests.post(f'{BASE_URL}/api/estop')
    print("\nStopped")
```

### Example 3: Home Assistant Integration

Add to `configuration.yaml`:

```yaml
rest_command:
  robot_control:
    url: "http://192.168.1.100/api/control"
    method: POST
    content_type: "application/json"
    payload: '{"throttle": {{ throttle }}, "steering": {{ steering }}}'

  robot_stop:
    url: "http://192.168.1.100/api/estop"
    method: POST

sensor:
  - platform: rest
    name: Robot Status
    resource: http://192.168.1.100/api/status
    json_attributes:
      - control_source
      - motor_left
      - motor_right
      - wifi_rssi
    value_template: '{{ value_json.status }}'
    scan_interval: 1

automation:
  - alias: "Robot Forward on Switch"
    trigger:
      platform: state
      entity_id: input_boolean.robot_go
      to: 'on'
    action:
      service: rest_command.robot_control
      data:
        throttle: 0.5
        steering: 0.0
```

### Example 4: Node-RED Flow

Import this flow to control robot from Node-RED:

```json
[
  {
    "id": "http_control",
    "type": "http request",
    "url": "http://192.168.1.100/api/control",
    "method": "POST",
    "paytoqs": false
  },
  {
    "id": "joystick_input",
    "type": "ui_joystick",
    "name": "Robot Control",
    "outputs": 1,
    "wires": [["format_command"]]
  },
  {
    "id": "format_command",
    "type": "function",
    "func": "msg.payload = {throttle: msg.payload.y, steering: msg.payload.x}; return msg;",
    "wires": [["http_control"]]
  }
]
```

## CORS (Cross-Origin Resource Sharing)

If accessing API from a web page on a different domain, CORS must be enabled.

**Current setting:** CORS enabled for all origins (permissive)

**Headers sent:**
```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

**To restrict origins**, edit `controller_http.c`:
```c
httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "https://mydomain.com");
```

## WebSocket Support (Future Enhancement)

For lower latency and persistent connection, WebSocket can be added:

**Benefits:**
- Bi-directional communication (ESP32 can push status updates)
- Lower overhead than HTTP polling
- ~10ms latency vs ~50ms for HTTP

**Implementation sketch:**
```c
// WebSocket endpoint: ws://192.168.1.100/ws
// Client sends: {"throttle":0.5,"steering":0.0}
// Server sends: {"status":"ok","motor_left":0.5,"motor_right":0.5}
```

*Not implemented in default firmware - see extension notes.*

## Configuration

Edit `firmware/main/config.h`:

```c
// HTTP API Configuration
#define HTTP_ENABLED          true       // Enable HTTP API module
#define HTTP_PORT             80         // Server port
#define HTTP_MAX_CONNECTIONS  4          // Max concurrent clients
#define HTTP_SERVE_WEB_UI     true       // Serve web interface at /
#define HTTP_RATE_LIMIT       50         // Max requests/sec per endpoint
#define HTTP_CORS_ENABLED     true       // Enable CORS headers

// Wi-Fi Configuration (or use menuconfig)
#define WIFI_SSID            "YourNetwork"
#define WIFI_PASSWORD        "YourPassword"
#define WIFI_MAX_RETRY       5           // Reconnect attempts
```

Or use `idf.py menuconfig`:
- Component config → Track Robot Configuration → Wi-Fi Settings

## Troubleshooting

**Problem:** Can't connect to ESP32 (connection refused)

**Solutions:**
- Check ESP32 is powered and booted (watch serial console)
- Verify Wi-Fi connected (check serial for IP address)
- Ping ESP32: `ping 192.168.1.100`
- Check firewall on client device
- Verify `HTTP_ENABLED` is `true` in config.h

**Problem:** Commands ignored (PS4 controller taking priority)

**Solutions:**
- Disconnect PS4 controller
- Change control priority in config.h (see above)
- Check `/api/status` for `"control_source"` field

**Problem:** High latency (>100ms)

**Solutions:**
- Check Wi-Fi signal strength (`wifi_rssi` in status)
- Move router closer to robot
- Use 5GHz Wi-Fi if available (less interference)
- Reduce HTTP request rate (don't exceed 20 req/sec)

**Problem:** Failsafe triggers too often

**Solutions:**
- Send commands faster (maintain <500ms interval)
- Increase timeout: `FAILSAFE_TIMEOUT_MS` in config.h
- Check network stability (packet loss?)

**Problem:** ESP32 reboots when using HTTP + motors

**Solutions:**
- **Critical:** Fix grounding (see wiring guide)
- Add capacitors to motor terminals (reduce EMI)
- Use shielded Ethernet cable for motor wires
- Increase buck converter capacity (may be voltage sag)

## Security Best Practices

Since there's no authentication:

1. **Network isolation**: Use separate SSID for robot (guest network)
2. **Firewall**: Block access from internet (router firewall)
3. **VPN**: Access from outside via VPN (not direct port forward)
4. **Monitoring**: Check `/api/status` regularly for unexpected control

**Adding API key (future):**
```c
// In controller_http.c, check header:
const char *api_key = httpd_req_get_hdr_value_str(req, "X-API-Key");
if (strcmp(api_key, "your_secret_key") != 0) {
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Invalid API key");
    return ESP_FAIL;
}
```

## Performance Metrics

**Measured latency (local Wi-Fi):**
- HTTP POST request: ~30-50ms (ESP32 → Router → Client)
- Status poll (GET): ~20-30ms
- Failsafe timeout: 500ms (configurable)

**Comparison:**
- PS4 BLE: ~10-20ms (lowest latency)
- HTTP: ~30-50ms (moderate latency)
- Serial: ~5ms (direct UART, lowest overhead)

**Recommendation:**
- For real-time RC control: Use PS4 controller
- For autonomous control: Use Serial (external MCU)
- For monitoring/testing: Use HTTP

## Next Steps

- [Serial Protocol](serial-protocol.md) - Lower-latency wired control
- [Architecture](architecture.md) - Understand control arbitration
- [Wiring Guide](wiring-guide.md) - Connect ESP32 to Wi-Fi router
