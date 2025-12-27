# Serial Control Protocol

## Overview

The Serial control module allows external devices (Raspberry Pi, Arduino, PC, etc.) to command the robot via UART. This is useful for:
- Autonomous control (run path planning on external MCU)
- Testing and debugging (command robot from terminal)
- Alternative to wireless control (wired connection)

**UART Configuration:**
- **Baud rate**: 115200
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Flow control**: None
- **GPIOs**: TX=GPIO20, RX=GPIO21 (UART1)

## Message Format

### JSON Protocol (Default)

Commands are sent as newline-terminated JSON objects.

**Format:**
```json
{"throttle": <float>, "steering": <float>, "estop": <bool>}
```

**Fields:**
- `throttle`: -1.0 to +1.0 (forward/reverse)
- `steering`: -1.0 to +1.0 (left/right turn)
- `estop`: `true` to trigger emergency stop, `false` to clear

**Example commands:**
```bash
# Forward half speed, no turn
{"throttle":0.5,"steering":0.0}

# Reverse full speed, turn right
{"throttle":-1.0,"steering":0.5}

# Emergency stop
{"estop":true}

# Clear e-stop and idle
{"throttle":0.0,"steering":0.0,"estop":false}
```

### Binary Protocol (Optional, Future Enhancement)

For lower latency and bandwidth, a binary protocol can be implemented:

**Packet structure (8 bytes):**
```
[HEADER][THROTTLE][STEERING][FLAGS][CHECKSUM]
  0x55     int16     int16     uint8    uint8

THROTTLE/STEERING: -1000 to +1000 (divide by 1000.0 for float)
FLAGS: Bit 0 = estop, Bit 1 = slow mode
CHECKSUM: XOR of all previous bytes
```

*Not implemented in default firmware - see extension notes below.*

## Control Arbitration

Serial control integrates with the multi-source control system:

**Default priority:** PS4 > HTTP > **Serial**

- If PS4 controller is active, Serial commands are ignored
- If PS4 disconnects (timeout), Serial can take control
- If PS4 reconnects, it regains priority

**To change priority**, edit `firmware/main/config.h`:
```c
#define CONTROL_SOURCE_PRIORITY_0  CONTROL_SOURCE_SERIAL  // Highest
#define CONTROL_SOURCE_PRIORITY_1  CONTROL_SOURCE_HTTP
#define CONTROL_SOURCE_PRIORITY_2  CONTROL_SOURCE_PS4     // Lowest
```

## Usage Examples

### Example 1: Linux/Mac Terminal

```bash
# Find ESP32 serial port
ls /dev/tty.usb*   # Mac
ls /dev/ttyUSB*    # Linux

# Send commands using echo
echo '{"throttle":0.5,"steering":0.0}' > /dev/ttyUSB1

# Or use screen for interactive control
screen /dev/ttyUSB1 115200
# Type JSON commands and press Enter
```

### Example 2: Python Script

```python
#!/usr/bin/env python3
import serial
import time
import json

# Open serial port
ser = serial.Serial('/dev/ttyUSB1', 115200, timeout=1)
time.sleep(2)  # Wait for ESP32 to initialize

def send_command(throttle, steering, estop=False):
    cmd = {
        "throttle": throttle,
        "steering": steering,
        "estop": estop
    }
    msg = json.dumps(cmd) + '\n'
    ser.write(msg.encode())
    print(f"Sent: {cmd}")

# Test sequence
send_command(0.0, 0.0)       # Stop
time.sleep(1)
send_command(0.3, 0.0)       # Slow forward
time.sleep(2)
send_command(0.0, 0.5)       # Turn right (in place)
time.sleep(1)
send_command(0.0, 0.0)       # Stop
time.sleep(1)
send_command(0.0, 0.0, estop=True)  # E-stop

ser.close()
```

### Example 3: Arduino as Master Controller

```cpp
// Arduino sends commands to ESP32 via Serial1
#include <ArduinoJson.h>

void setup() {
  Serial1.begin(115200);  // TX to ESP32 GPIO21, RX from ESP32 GPIO20
  delay(1000);
}

void sendRobotCommand(float throttle, float steering) {
  StaticJsonDocument<128> doc;
  doc["throttle"] = throttle;
  doc["steering"] = steering;
  doc["estop"] = false;

  serializeJson(doc, Serial1);
  Serial1.println();  // Newline terminator
}

void loop() {
  // Simple patrol pattern
  sendRobotCommand(0.5, 0.0);  // Forward
  delay(2000);
  sendRobotCommand(0.0, 1.0);  // Turn right
  delay(1000);
  sendRobotCommand(0.5, 0.0);  // Forward
  delay(2000);
  sendRobotCommand(0.0, -1.0); // Turn left
  delay(1000);
}
```

### Example 4: Raspberry Pi with Joystick

```python
#!/usr/bin/env python3
import serial
import json
import pygame

# Initialize pygame joystick
pygame.init()
pygame.joystick.init()
joystick = pygame.joystick.Joystick(0)
joystick.init()

# Open serial to ESP32
ser = serial.Serial('/dev/ttyAMA0', 115200)  # Pi GPIO UART

def map_joystick():
    pygame.event.pump()
    throttle = -joystick.get_axis(1)  # Left stick Y (inverted)
    steering = joystick.get_axis(2)   # Right stick X
    estop = joystick.get_button(0)    # Button A
    return throttle, steering, estop

try:
    while True:
        throttle, steering, estop = map_joystick()
        cmd = {"throttle": throttle, "steering": steering, "estop": bool(estop)}
        ser.write((json.dumps(cmd) + '\n').encode())
        pygame.time.wait(20)  # 50Hz update rate
except KeyboardInterrupt:
    ser.close()
```

## Response Messages (ESP32 → Host)

The ESP32 can send status messages back over serial (for debugging/monitoring):

**Format:**
```json
{"status":"ok","source":"serial","left":0.5,"right":0.5}
```

**Fields:**
- `status`: "ok", "estop", "failsafe"
- `source`: "ps4", "http", "serial" (active control source)
- `left`: Current left motor command (-1.0 to +1.0)
- `right`: Current right motor command (-1.0 to +1.0)

**Example:**
```bash
# Read responses (Linux)
cat /dev/ttyUSB1
{"status":"ok","source":"serial","left":0.50,"right":0.50}
{"status":"ok","source":"serial","left":0.00,"right":0.00}
{"status":"estop","source":"ps4","left":0.00,"right":0.00}
```

*Note: Response messages are optional and can be disabled to reduce UART traffic (edit `SERIAL_SEND_STATUS` in config.h).*

## CLI Test Mode

When no external controller is connected, the Serial interface provides a simple CLI for testing:

```
Track Robot v1.0.0
Type 'help' for commands

> help
Commands:
  fwd <speed>      - Move forward (0.0 to 1.0)
  rev <speed>      - Move reverse (0.0 to 1.0)
  left <speed>     - Turn left (0.0 to 1.0)
  right <speed>    - Turn right (0.0 to 1.0)
  stop             - Stop motors
  estop            - Emergency stop (latch)
  enable           - Clear e-stop
  status           - Print current state
  help             - This message

> fwd 0.5
Moving forward at 50%

> stop
Motors stopped

> status
Status: OK
Source: Serial
Left:  0.00
Right: 0.00
E-stop: false
```

**To enable CLI mode**, set `SERIAL_CLI_MODE` in `config.h`:
```c
#define SERIAL_CLI_MODE  true   // Enable CLI test commands
```

## Error Handling

### Malformed JSON
**Input:** `{"throttle":0.5,"steering":INVALID}`

**Response:**
```json
{"error":"json_parse_failed"}
```

**Behavior:** Command ignored, motors maintain last valid state

### Out of Range Values
**Input:** `{"throttle":5.0,"steering":0.0}`

**Response:**
```json
{"error":"value_out_of_range","throttle":5.0}
```

**Behavior:** Values clamped to [-1.0, +1.0], command applied with clamped values

### Buffer Overflow
**Input:** JSON message >256 characters

**Response:**
```json
{"error":"message_too_long"}
```

**Behavior:** Message discarded, motors maintain last valid state

### Failsafe
**Condition:** No valid serial command received for 500ms (configurable)

**Behavior:**
- Motors stop
- Status changes to "failsafe"
- Waits for new command

**Recovery:** Send any valid command

## Configuration

Edit `firmware/main/config.h`:

```c
// Serial Control Configuration
#define SERIAL_ENABLED           true      // Enable serial control module
#define SERIAL_UART_NUM          UART_NUM_1 // UART port number
#define SERIAL_TX_PIN            GPIO_NUM_20
#define SERIAL_RX_PIN            GPIO_NUM_21
#define SERIAL_BAUD_RATE         115200
#define SERIAL_BUF_SIZE          256       // RX buffer size
#define SERIAL_CLI_MODE          false     // Enable CLI test mode
#define SERIAL_SEND_STATUS       true      // Send status messages
#define SERIAL_STATUS_RATE_MS    100       // Status update rate (ms)
```

## Wiring

| ESP32-C5 Pin | External Device | Notes |
|--------------|-----------------|-------|
| GPIO20 (TX) | RX | ESP32 transmit → Device receive |
| GPIO21 (RX) | TX | ESP32 receive ← Device transmit |
| GND | GND | **Must share common ground** |

**Important:**
- ESP32 GPIO is 3.3V logic
- If external device uses 5V logic (e.g., Arduino Uno), use **level shifter**
- Raspberry Pi uses 3.3V logic (direct connection OK)

**Level shifter example (if needed):**
```
ESP32 GPIO20 (3.3V) ──[Level Shifter]── Arduino RX (5V)
ESP32 GPIO21 (3.3V) ──[Level Shifter]── Arduino TX (5V)
ESP32 GND ───────────────────────────── Arduino GND
```

## Performance

**Latency:**
- Command received → Motors respond: ~5-10ms
- Faster than HTTP (~50ms) but slower than PS4 BLE (~10-20ms)

**Throughput:**
- 115200 baud = ~11.5 KB/sec
- JSON command (~50 bytes) = ~230 commands/sec max
- Practical rate: 50Hz (20ms interval) recommended

**Reliability:**
- No checksum in JSON mode (rely on JSON parser validation)
- For mission-critical control, implement binary protocol with CRC

## Implementing Binary Protocol (Advanced)

If JSON overhead is too high, implement binary protocol:

**1. Define packet structure** in `controller_serial.c`:
```c
typedef struct __attribute__((packed)) {
    uint8_t header;       // 0x55
    int16_t throttle;     // -1000 to +1000
    int16_t steering;     // -1000 to +1000
    uint8_t flags;        // Bit 0: estop, Bit 1: slow mode
    uint8_t checksum;     // XOR of bytes 0-6
} serial_packet_t;
```

**2. Parse binary in serial RX handler**:
```c
if (buf[0] == 0x55 && len == sizeof(serial_packet_t)) {
    serial_packet_t *pkt = (serial_packet_t*)buf;
    uint8_t crc = calc_xor_checksum(buf, 7);
    if (crc == pkt->checksum) {
        cmd.throttle = pkt->throttle / 1000.0f;
        cmd.steering = pkt->steering / 1000.0f;
        cmd.estop = pkt->flags & 0x01;
    }
}
```

**3. Send from host** (Python example):
```python
import struct
packet = struct.pack('<Bhhbb',
    0x55,              # Header
    500,               # Throttle (0.5 * 1000)
    0,                 # Steering (0.0 * 1000)
    0,                 # Flags
    0                  # Checksum (calculate)
)
checksum = 0
for b in packet[:-1]:
    checksum ^= b
packet = packet[:-1] + bytes([checksum])
ser.write(packet)
```

## Troubleshooting

**Problem:** No response from ESP32

**Solutions:**
- Check wiring (TX/RX crossed correctly?)
- Verify baud rate (both ends must be 115200)
- Use USB cable with data lines (not charge-only)
- Check `SERIAL_ENABLED` is `true` in config.h

**Problem:** Garbled characters

**Solutions:**
- Check common ground (GND) between devices
- Use level shifter if voltage mismatch (3.3V vs 5V)
- Reduce baud rate to 57600 if long cables (>1m)

**Problem:** Commands ignored (PS4 controller taking priority)

**Solutions:**
- Disconnect PS4 controller or disable PS4 module
- Change control priority in config.h (see above)
- Check serial console for "Source: serial" to confirm active

**Problem:** Failsafe triggers too often

**Solutions:**
- Increase timeout: `FAILSAFE_TIMEOUT_MS` in config.h
- Send commands faster (reduce delay between messages)
- Check for message drops (serial buffer overflow?)

## Next Steps

- [HTTP API Control](http-api.md) - Alternative wireless control
- [Architecture Overview](architecture.md) - Understand control arbitration
- [Wiring Guide](wiring-guide.md) - Connect external UART device
