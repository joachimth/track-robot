# Tracked Robot Firmware (ESP32-S3)

Professional firmware for a tracked robot car controlled via **PS3 controller** (Bluetooth Classic), HTTP API, or Serial commands. Runs on ESP32-S3 (Heltec WiFi Kit 32 V3) with dual BTS7960 motor drivers.

> **✅ Hardware**: This project uses **Heltec WiFi Kit 32 V3** (ESP32-S3) which supports Bluetooth Classic for PS3 controllers. See [docs/architecture.md](docs/architecture.md) for details.

## Quick Start

### 1. Hardware Wiring

| Component | Pin | ESP32-S3 GPIO | Notes |
|-----------|-----|---------------|-------|
| **Left Motor BTS7960** | RPWM | GPIO4 | PWM forward |
| | LPWM | GPIO5 | PWM reverse |
| | R_EN | GPIO6 | Enable high |
| | L_EN | GPIO7 | Enable high |
| **Right Motor BTS7960** | RPWM | GPIO8 | PWM forward |
| | LPWM | GPIO9 | PWM reverse |
| | R_EN | GPIO10 | Enable high |
| | L_EN | GPIO11 | Enable high |
| **Status LED** | Anode | GPIO18 | Optional |
| **Serial Control** | TX | GPIO20 | UART1 (optional) |
| | RX | GPIO21 | UART1 (optional) |

**Power Wiring:**
```
Milwaukee 12V Battery
  ├─→ BTS7960 #1 VCC (B+/B-)
  ├─→ BTS7960 #2 VCC (B+/B-)
  └─→ Buck Converter (12V → 5V)
       └─→ ESP32-S3 5V input (or USB-C)
       └─→ BTS7960 VCC (logic, 5V)

Common Ground: Battery (-) ─ BTS7960 GND ─ Buck GND ─ ESP32-S3 GND
```

- **Fuse**: 30A on 12V battery line (recommended)
- **Wire gauge**: 14 AWG minimum for motor power
- **Enable pins**: Tie BTS7960 R_EN and L_EN to 5V or control via ESP32

### 2. Flash Firmware

#### Option A: Web Flasher (Easiest)
1. Go to **[https://joachimth.github.io/track-robot/](https://joachimth.github.io/track-robot/)** (GitHub Pages)
2. Connect ESP32-S3 (Heltec WiFi Kit 32 V3) via USB-C
3. Click "Connect" and select serial port
4. Click "Install" and wait (~2 min)

#### Option B: ESP-IDF Command Line
```bash
cd firmware

# Install PS3 library first (required)
git clone https://github.com/jvpernis/esp32-ps3.git components/ps3
# OR for ESP-IDF v5 compatibility:
# git clone https://github.com/NSUHackspace/esp32-ps3-esp-idf-v5.git components/ps3

idf.py set-target esp32s3
idf.py menuconfig  # Configure Wi-Fi credentials
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### 3. Configure Wi-Fi (for HTTP control)

Edit `firmware/main/config.h` before building:
```c
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
```

Or use `idf.py menuconfig` → Component config → Track Robot Configuration

### 4. Pair PS3 Controller

1. Power on ESP32-S3 (watch serial console for "BT Address: XX:XX:XX:XX:XX:XX")
2. Pair PS3 controller using PC:
   - **Windows**: Use [SixaxisPairTool](http://www.dancingpixelstudios.com/sixaxis-pair-tool/)
   - **Linux/Mac**: Use sixpair utility
   - Connect PS3 controller via USB
   - Set "Master" address to ESP32's Bluetooth MAC (from serial console)
   - Disconnect USB
3. Press **PS button** on controller - LED 1 should light up solid
4. Test controls (keep robot on blocks, wheels off ground):
   - **Left Stick Y**: Forward/backward
   - **Right Stick X**: Steering (turn)
   - **X Button**: Emergency stop (motors off)
   - **Start Button**: Resume after e-stop
   - **Triangle Button**: Toggle slow mode (50% speed limit)

### 5. Alternative Control Methods

**Serial Control (UART):**
```bash
# Connect to GPIO20/21 at 115200 baud
echo '{"throttle":0.5,"steering":0.0}' > /dev/ttyUSB1
```
See [docs/serial-protocol.md](docs/serial-protocol.md)

**HTTP API:**
```bash
# Find ESP32 IP from serial console
curl http://192.168.1.xxx/api/control -X POST \
  -H "Content-Type: application/json" \
  -d '{"throttle":0.5,"steering":0.0}'
```
See [docs/http-api.md](docs/http-api.md)

## Safety Features

- **Emergency Stop**: X button on controller, or send e-stop command via API
- **Failsafe**: Motors stop if no command received for 500ms
- **Deadzone**: 5% stick deadzone prevents drift
- **Soft Start**: Ramping prevents drivetrain shock and current spikes
- **Slow Mode**: Triangle button limits speed to 50%

**⚠️ First Test**: Keep tracks off the ground! Press X (e-stop) before connecting battery.

## Configuration

Edit `firmware/main/config.h` to customize:
- PWM frequency (default 20kHz)
- Pin mapping
- Deadzone, expo, max speed
- Failsafe timeout
- Enable/disable control modules (PS4, Serial, HTTP)

See [docs/pwm-configuration.md](docs/pwm-configuration.md) for tuning guidance.

## Troubleshooting

**PS3 Controller won't pair:**
- Ensure ESP32-S3 is powered and Bluetooth initialized (check serial logs)
- Verify you set the correct MAC address using SixaxisPairTool/sixpair
- Reset controller: paperclip in small hole on back for 5 sec
- Try a different USB cable when pairing (must be data cable)
- Check serial console - should show "PS3 controller initialized, waiting for connection..."

**Motors don't move:**
- Check BTS7960 enable pins (must be HIGH, 5V)
- Verify 12V power to BTS7960 VCC/GND
- Check common ground between ESP32 and drivers
- Press Start button (may be in e-stop state)

**Web flasher not working:**
- Use Chrome or Edge (Firefox/Safari unsupported)
- Install USB drivers (CP210x or CH340)
- Try different USB cable (must be data cable, not charge-only)

## Documentation

- [Architecture Overview](docs/architecture.md) - System design and module structure
- [PWM Configuration](docs/pwm-configuration.md) - Frequency tuning and motor control
- [Wiring Guide](docs/wiring-guide.md) - Detailed BTS7960 hookup and common pitfalls
- [Serial Protocol](docs/serial-protocol.md) - UART command format
- [HTTP API](docs/http-api.md) - REST endpoint reference
- [Release Workflow](docs/release-workflow.md) - How builds and releases work

## Development

**Build from source:**
```bash
git clone https://github.com/joachimth/track-robot.git
cd track-robot/firmware

# Install PS3 library (required)
git clone https://github.com/jvpernis/esp32-ps3.git components/ps3

idf.py set-target esp32s3
idf.py build flash monitor
```

**Create release:**
```bash
git tag v1.0.0
git push origin v1.0.0
# GitHub Actions builds and publishes firmware binaries
```

## License

MIT License - see LICENSE file

## Credits

Built with ESP-IDF v5.3+, designed for Heltec WiFi Kit 32 V3 (ESP32-S3) and BTS7960 motor drivers.
