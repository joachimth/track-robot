# Architecture Overview

## System Design Philosophy

This firmware is designed for **modularity**, **safety**, and **extensibility**. Each control path (PS3, Serial, HTTP) is a first-class citizen that can be enabled/disabled independently. The architecture supports future expansion (e.g., encoders, sensors, autonomous modes).

## Hardware Platform: ESP32-S3 (Heltec WiFi Kit 32 V3)

**This firmware runs on ESP32-S3 which supports Bluetooth Classic for PS3 controllers.**

- **PS3 controllers** use Bluetooth Classic (HID profile) → ✅ Fully compatible with ESP32-S3
- **Hardware**: Heltec WiFi Kit 32 V3 (ESP32-S3 dual-core, 8MB Flash, 8MB PSRAM)
- **Bluetooth**: Dual-mode (BLE + Classic) - ideal for PS3 controllers

**Why ESP32-S3:**
1. Bluetooth Classic support (required for PS3)
2. Dual-core performance for concurrent Wi-Fi + Bluetooth
3. Large memory (8MB PSRAM) for future expansion
4. USB-C interface for easy programming and power

This firmware implements **PS3 controller support** via Bluetooth Classic using the jvpernis/esp32-ps3 library.

## Module Structure

```
┌─────────────────────────────────────────────────────────┐
│                      main.c                             │
│  (Initialization, Task Orchestration, Safety Monitor)   │
└────────────┬────────────────────────────────────────────┘
             │
      ┌──────┴──────┬──────────────┬─────────────┐
      │             │              │             │
┌─────▼─────┐ ┌────▼────┐ ┌───────▼──────┐ ┌────▼──────┐
│ PS3       │ │ Serial  │ │ HTTP         │ │ Safety/   │
│ Controller│ │ Control │ │ API          │ │ Failsafe  │
│ (BT)      │ │ (UART)  │ │ (Wi-Fi)      │ │           │
└─────┬─────┘ └────┬────┘ └───────┬──────┘ └────┬──────┘
      │            │              │             │
      └────────────┴──────────────┴─────────────┘
                   │
            ┌──────▼───────┐
            │ Differential │
            │ Drive Mixer  │
            └──────┬───────┘
                   │
         ┌─────────┴─────────┐
         │                   │
    ┌────▼─────┐       ┌────▼─────┐
    │ Left     │       │ Right    │
    │ Motor    │       │ Motor    │
    │ BTS7960  │       │ BTS7960  │
    └──────────┘       └──────────┘
```

## Control Flow

### 1. Input Sources
Each control module runs independently and writes to a shared `control_command_t` structure:
```c
typedef struct {
    float throttle;  // -1.0 to +1.0 (fwd/rev)
    float steering;  // -1.0 to +1.0 (left/right)
    bool estop;      // Emergency stop flag
    uint32_t timestamp;  // millis() of last update
} control_command_t;
```

### 2. Arbitration Logic
The safety module determines which input source to use:

**Priority (configurable in config.h):**
1. **Emergency Stop**: If any source sets `estop=true`, motors stop immediately (latched until cleared)
2. **Failsafe Timeout**: If `(current_time - timestamp) > FAILSAFE_TIMEOUT_MS`, motors stop
3. **Source Priority**: PS4 > HTTP > Serial (default order, configurable)
   - "Higher priority source wins"
   - If higher priority source times out, fall back to next

**Example:**
- PS4 connected and active → PS4 controls robot
- PS4 disconnects (timeout) → HTTP takes over (if active)
- PS4 reconnects → PS4 takes control again

### 3. Differential Drive Mixer
Converts `(throttle, steering)` → `(left_motor, right_motor)` using tank mixing:

```
left_motor  = throttle + steering
right_motor = throttle - steering

Then clamp to [-1.0, +1.0] and apply:
- Deadzone (ignore inputs < 5%)
- Expo curve (reduce sensitivity near center)
- Speed limiting (slow mode, max speed cap)
```

See `mixer_diffdrive.c` for implementation.

### 4. Motor Control
Each BTS7960 driver receives:
- **Direction**: Forward (RPWM active, LPWM=0) or Reverse (LPWM active, RPWM=0)
- **PWM duty**: 0-100% at configured frequency (20kHz default)
- **Slew rate limiting**: Ramps PWM changes to prevent current spikes

**Safety features:**
- **Braking vs Coasting**: When stopping, either short-circuit motor (active braking) or coast
- **Enable control**: Can disable outputs entirely (emergency stop)
- **Per-side inversion**: Swap motor polarity in config if wired backwards

See `motor_bts7960.c` for implementation.

## Configuration System

Uses ESP-IDF Kconfig + `config.h` for compile-time settings:

**Kconfig (menuconfig):**
- Wi-Fi credentials
- Feature enables (PS4/Serial/HTTP)
- Build options

**config.h:**
- Pin assignments
- PWM frequency
- Control parameters (deadzone, expo, limits)
- Timeouts and safety thresholds

**Why both?**
- Kconfig: User-facing settings that change per deployment (Wi-Fi, features)
- config.h: Hardware/tuning parameters that rarely change

## Safety & Failsafe

### Emergency Stop (E-Stop)
- **Trigger**: PS4 X button, or API/Serial command
- **Behavior**: Immediately stop motors, latch flag
- **Recovery**: Press PS4 Start button, or send "enable" command
- **Indicator**: Status LED blinks rapidly

### Failsafe Timeout
- **Default**: 500ms (configurable)
- **Behavior**: If no valid control command received within timeout, stop motors
- **Recovery**: Automatic when new commands resume
- **Indicator**: Status LED fades slowly

### Soft Start / Slew Rate Limiting
- **Purpose**: Prevent mechanical shock and current spikes
- **Implementation**: Limit PWM change rate (e.g., max 10% duty change per 20ms)
- **Configurable**: `MOTOR_RAMP_RATE_PERCENT_PER_MS` in config.h

### Safe Test Mode
- **Recommendation**: Test with tracks off the ground
- **Feature**: Optional "ARM" button requirement before motors can move (future enhancement)

## Future Expansion Hooks

The architecture supports adding:

### Odometry / Encoders
- Add `encoder_left.c` and `encoder_right.c` modules
- Wire external quadrature encoders to available GPIOs
- Publish encoder ticks to mixer for speed control (PID velocity loop)

### Autonomous Control
- Add `controller_autonomous.c` module
- Takes control priority when activated
- Uses sensor inputs (encoders, IMU, etc.) for navigation

### Telemetry / Logging
- HTTP API already provides status endpoint
- Extend to log encoder ticks, motor currents, battery voltage
- Add SD card module for data logging

### External MCU Control
- Serial protocol is designed for external controller (e.g., Raspberry Pi, Jetson)
- Can run vision/AI on external MCU, send throttle/steering commands via UART

## Build System

Uses ESP-IDF CMake build system:
- `firmware/CMakeLists.txt` - Top-level project config
- `firmware/main/CMakeLists.txt` - Main component config
- Dependencies auto-managed by ESP-IDF component manager

**Partitions:**
- Factory app: ~1.5MB (firmware)
- OTA: Not used initially (future: over-the-air updates)
- NVS: 16KB (for Wi-Fi credentials, calibration)

See `firmware/partitions.csv`

## Performance Considerations

### CPU Usage
- **Core 0**: Bluetooth + Wi-Fi stack (ESP-IDF default)
- **Core 1**: Main control loop, motor PWM
- **RTOS Tasks**: Each module runs in separate FreeRTOS task
- **Priority**: Safety/failsafe = highest, control input = high, motor = high, HTTP = low

### Memory
- **SRAM**: ~400KB available (ESP32-S3 has 512KB total)
- **PSRAM**: 8MB available (for future features: logging, vision)
- **Flash**: 8MB (plenty for firmware + future OTA)

### Latency
- **PS3 Bluetooth Classic**: ~15-30ms input latency
- **HTTP**: ~50-100ms (Wi-Fi + TCP overhead)
- **Serial**: ~5ms (direct UART, minimal overhead)
- **Control loop**: 20ms (50Hz update rate)

### Power Consumption
- **ESP32-S3 idle**: ~20mA @ 5V
- **ESP32-S3 active (Bluetooth Classic + Wi-Fi)**: ~100-180mA @ 5V
- **Motors**: ~5-30A @ 12V (depending on load)

**Battery life estimate (12V 6Ah):**
- Light use (10A avg): ~35 min
- Heavy use (20A avg): ~18 min
- ESP32 power negligible compared to motors

## Next Steps

Read the detailed docs:
- [PWM Configuration](pwm-configuration.md) - Tune motor control
- [Wiring Guide](wiring-guide.md) - Hardware hookup
- [Serial Protocol](serial-protocol.md) - UART command format
- [HTTP API](http-api.md) - REST endpoints
