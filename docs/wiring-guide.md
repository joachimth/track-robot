# Wiring Guide

## Safety First ⚠️

- **Disconnect battery** before making any wiring changes
- **Use a fuse** (30A recommended) on the 12V battery positive line
- **Verify polarity** before connecting (reverse polarity can destroy components)
- **Test with multimeter** before powering on
- **Keep tracks off ground** during initial testing

## Bill of Materials

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32-C5 Dev Board (Waveshare) | 1 | Must be ESP32-C5, not C3/C6 |
| BTS7960 43A H-Bridge Driver | 2 | One per motor |
| DC Motor (Topran 108 792) | 2 | 12V windshield wiper motors |
| Milwaukee M12 Battery | 1 | 12V 4Ah or 6Ah |
| Buck Converter (12V→5V) | 1 | 3A minimum output |
| Fuse Holder + 30A Fuse | 1 | Inline on battery + line |
| Wire 14 AWG | 2m | For motor power (red + black) |
| Wire 22 AWG | 2m | For signals and logic (various colors) |
| Dupont Connectors | ~30 | Female-to-female jumpers |
| XT60 Connectors | 2 | Battery and power distribution |
| Heat Shrink Tubing | Assorted | Insulate connections |
| Optional: Status LED | 1 | 5mm red LED + 220Ω resistor |

## Power Distribution Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│  Milwaukee M12 Battery (12V 6Ah)                                │
│  ┌─────┐                                                        │
│  │ (+) │──────[30A Fuse]────┬───────────┬───────────┐          │
│  │ (-) │────────────────────┼───────────┼───────────┼─── GND   │
│  └─────┘                    │           │           │          │
└────────────────────────────┼───────────┼───────────┼──────────┘
                              │           │           │
                              │           │           │
                   ┌──────────▼──┐   ┌────▼──────┐   │
                   │ BTS7960 #1  │   │ BTS7960 #2│   │
                   │ (Left Motor)│   │(Right Mot)│   │
                   │             │   │           │   │
                   │ B+  B-      │   │ B+  B-    │   │
                   │ 12V 12V     │   │ 12V 12V   │   │
                   │ VCC GND     │   │ VCC GND   │   │
                   │ 5V  GND     │   │ 5V  GND   │   │
                   └──────┬──────┘   └─────┬─────┘   │
                          │                │         │
                          └────────┬───────┘         │
                                   │                 │
                              ┌────▼─────────────────▼────┐
                              │  Buck Converter 12V→5V    │
                              │  Input: 12V (from battery)│
                              │  Output: 5V 3A            │
                              │                           │
                              │  OUT+  OUT-               │
                              └────┬─────┬────────────────┘
                                   │     │
                                   │     └──────────────┐
                              ┌────▼────────────┐       │
                              │  ESP32-C5       │       │
                              │  5V   GND       │       │
                              └─────────────────┘       │
                                                        │
                              ┌─────────────────────────┘
                              │
                          Star Ground Point
                           (Common GND)
```

## Star Ground Topology (Critical!)

**The most common cause of ESP32 crashes is ground loops and noise from motors.**

**CORRECT star ground:**
```
Battery GND (thick 14AWG wire)
  ├─→ BTS7960 #1 GND (short, thick wire)
  ├─→ BTS7960 #2 GND (short, thick wire)
  └─→ Buck Converter GND (medium wire)
       └─→ ESP32 GND (thin wire, single connection point)
```

**WRONG (ground loop):**
```
❌ Battery → BTS7960 #1 → BTS7960 #2 → Buck → ESP32
   (Creates loop, motor noise couples into ESP32)
```

**Why this matters:**
- Motor switching creates voltage spikes on ground
- If ESP32 shares motor ground path, spikes reset the MCU
- Star topology isolates ESP32 from motor noise

## Pin Connections

### BTS7960 #1 (Left Motor)

| BTS7960 Pin | Connect To | Wire | Notes |
|-------------|------------|------|-------|
| **Power** | | | |
| B+ | Battery (+) via fuse | Red 14AWG | High current |
| B- | Battery (-) | Black 14AWG | High current |
| VCC (logic) | Buck 5V output | Red 22AWG | Logic power |
| GND (logic) | Buck GND | Black 22AWG | Star ground |
| **Motor Output** | | | |
| M+ | Left motor (+) | Red 14AWG | Motor terminal |
| M- | Left motor (-) | Black 14AWG | Motor terminal |
| **Control Signals (to ESP32)** | | | |
| RPWM | ESP32 GPIO4 | Yellow 22AWG | PWM forward |
| LPWM | ESP32 GPIO5 | Green 22AWG | PWM reverse |
| R_EN | Buck 5V (or GPIO6) | Orange 22AWG | Enable (tie HIGH) |
| L_EN | Buck 5V (or GPIO7) | Blue 22AWG | Enable (tie HIGH) |
| R_IS | Not connected | - | Current sense (optional) |
| L_IS | Not connected | - | Current sense (optional) |

### BTS7960 #2 (Right Motor)

| BTS7960 Pin | Connect To | Wire | Notes |
|-------------|------------|------|-------|
| **Power** | | | |
| B+ | Battery (+) via fuse | Red 14AWG | High current |
| B- | Battery (-) | Black 14AWG | High current |
| VCC (logic) | Buck 5V output | Red 22AWG | Logic power |
| GND (logic) | Buck GND | Black 22AWG | Star ground |
| **Motor Output** | | | |
| M+ | Right motor (+) | Red 14AWG | Motor terminal |
| M- | Right motor (-) | Black 14AWG | Motor terminal |
| **Control Signals (to ESP32)** | | | |
| RPWM | ESP32 GPIO8 | Yellow 22AWG | PWM forward |
| LPWM | ESP32 GPIO9 | Green 22AWG | PWM reverse |
| R_EN | Buck 5V (or GPIO10) | Orange 22AWG | Enable (tie HIGH) |
| L_EN | Buck 5V (or GPIO11) | Blue 22AWG | Enable (tie HIGH) |
| R_IS | Not connected | - | Current sense (optional) |
| L_IS | Not connected | - | Current sense (optional) |

**Enable Pin Options:**
1. **Tie to 5V** (simplest): Drivers always enabled, ESP32 controls via PWM only
2. **Connect to GPIO** (recommended): Allows emergency stop by pulling enable LOW

### ESP32-C5 Connections

| ESP32 Pin | Function | Connect To | Notes |
|-----------|----------|------------|-------|
| **Power** | | | |
| 5V | Power input | Buck 5V output | Via USB or 5V pin |
| GND | Ground | Buck GND | Single point! |
| **Motor Control** | | | |
| GPIO4 | Left RPWM | BTS7960 #1 RPWM | LEDC Channel 0 |
| GPIO5 | Left LPWM | BTS7960 #1 LPWM | LEDC Channel 1 |
| GPIO6 | Left R_EN | BTS7960 #1 R_EN | Optional (or tie to 5V) |
| GPIO7 | Left L_EN | BTS7960 #1 L_EN | Optional (or tie to 5V) |
| GPIO8 | Right RPWM | BTS7960 #2 RPWM | LEDC Channel 2 |
| GPIO9 | Right LPWM | BTS7960 #2 LPWM | LEDC Channel 3 |
| GPIO10 | Right R_EN | BTS7960 #2 R_EN | Optional (or tie to 5V) |
| GPIO11 | Right L_EN | BTS7960 #2 L_EN | Optional (or tie to 5V) |
| **Status** | | | |
| GPIO18 | Status LED | LED anode (+) | Via 220Ω resistor to GND |
| **Serial Control (Optional)** | | | |
| GPIO20 | UART1 TX | External RX | For external controller |
| GPIO21 | UART1 RX | External TX | For external controller |
| **USB (Programming)** | | | |
| GPIO12/13 | USB D+/D- | USB cable | Built-in on dev board |

**Avoid these GPIOs (reserved):**
- GPIO0-3: Strapping pins (boot mode selection)
- GPIO12-13: USB (if using USB for programming)
- GPIO16-17: UART0 (console logging)

## Step-by-Step Wiring Procedure

### Step 1: Prepare Components
1. Lay out all components on non-conductive surface
2. Label each BTS7960 module (Left/Right) with masking tape
3. Cut wire to length (leave extra, you can trim later):
   - Power (14AWG): ~30cm per connection
   - Signals (22AWG): ~15cm per connection

### Step 2: Power Distribution (Battery Disconnected!)
1. **Install fuse**: 30A inline fuse on battery (+) line
2. **Connect battery (+) to distribution block** (or solder junction)
3. **Connect battery (-) to star ground point**
4. **From distribution block (+)**:
   - To BTS7960 #1 B+ (14AWG red)
   - To BTS7960 #2 B+ (14AWG red)
   - To Buck converter input (+) (14AWG red)
5. **From star ground point (-)**:
   - To BTS7960 #1 B- (14AWG black)
   - To BTS7960 #2 B- (14AWG black)
   - To Buck converter input (-) (14AWG black)
6. **Verify polarity** with multimeter (battery still disconnected)

### Step 3: Buck Converter Setup
1. **Before connecting load**: Adjust buck output voltage
   - Connect 12V bench supply to buck input (if available)
   - Measure output with multimeter
   - Adjust potentiometer until output = 5.0V ± 0.1V
2. **Connect buck output (+)** to:
   - ESP32-C5 5V pin (or USB 5V pad)
   - BTS7960 #1 VCC (logic power)
   - BTS7960 #2 VCC (logic power)
3. **Connect buck output (-)** to:
   - ESP32-C5 GND
   - BTS7960 #1 GND (logic)
   - BTS7960 #2 GND (logic)

### Step 4: Motor Connections
1. **BTS7960 #1 → Left Motor**:
   - M+ to motor terminal 1 (14AWG red)
   - M- to motor terminal 2 (14AWG black)
2. **BTS7960 #2 → Right Motor**:
   - M+ to motor terminal 1 (14AWG red)
   - M- to motor terminal 2 (14AWG black)
3. **Note polarity**: If motor runs backwards, swap M+ and M- (or flip in config)

### Step 5: Signal Wiring (ESP32 to BTS7960)
1. **Left motor signals**:
   - GPIO4 → BTS7960 #1 RPWM (yellow)
   - GPIO5 → BTS7960 #1 LPWM (green)
   - GPIO6 → BTS7960 #1 R_EN (orange, or tie to 5V)
   - GPIO7 → BTS7960 #1 L_EN (blue, or tie to 5V)
2. **Right motor signals**:
   - GPIO8 → BTS7960 #2 RPWM (yellow)
   - GPIO9 → BTS7960 #2 LPWM (green)
   - GPIO10 → BTS7960 #2 R_EN (orange, or tie to 5V)
   - GPIO11 → BTS7960 #2 L_EN (blue, or tie to 5V)
3. **Status LED** (optional):
   - GPIO18 → 220Ω resistor → LED anode (+)
   - LED cathode (-) → GND

### Step 6: Pre-Power Checks
1. **Multimeter continuity test**:
   - Battery (+) should reach BTS7960 B+, buck input (+)
   - Battery (-) should reach all GND points
   - NO continuity between (+) and (-) (check for shorts!)
2. **Visual inspection**:
   - No bare wire exposed (use heat shrink)
   - Connections mechanically secure (no loose wires)
   - Correct polarity on all components
3. **Resistance test** (battery disconnected):
   - Between B+ and B- on BTS7960: Should be >1kΩ (open circuit)
   - If <100Ω, there's a short (DO NOT POWER ON)

### Step 7: First Power-Up (Motors Disconnected)
1. **Disconnect motors** from BTS7960 (M+ and M- open)
2. **Connect battery** (should hear/see no sparks - if sparks, disconnect immediately!)
3. **Measure voltages**:
   - Battery terminals: ~12V
   - Buck output: 5.0V ± 0.2V
   - ESP32 5V pin: 5.0V ± 0.2V
   - BTS7960 VCC: 5.0V ± 0.2V
4. **Check ESP32 boots**: Status LED should blink, or connect USB to see serial output
5. **If anything smokes/overheats**: Disconnect battery immediately!

### Step 8: Motor Test (Tracks Off Ground!)
1. **Reconnect motors** to BTS7960 M+/M-
2. **Elevate robot** so tracks are off the ground
3. **Power on** and connect PS4 controller (or use Serial/HTTP control)
4. **Gently test throttle**: Motors should spin slowly
5. **Verify direction**:
   - Left stick forward → both tracks forward
   - Left stick back → both tracks reverse
   - Right stick right → right track faster (robot turns right)
   - If backwards, edit `config.h` and flip `MOTOR_LEFT_INVERT` or `MOTOR_RIGHT_INVERT`

### Step 9: Final Assembly
1. **Cable management**: Zip-tie wires to robot chassis
2. **Secure components**: Mount ESP32, BTS7960, buck converter with standoffs or velcro
3. **Protect connections**: Apply heat shrink or electrical tape to all solder joints
4. **Label wires**: Mark battery (+) and (-) clearly
5. **Test e-stop**: Press X button on controller, verify motors stop immediately

## Common Wiring Mistakes

### ❌ Mistake: BTS7960 VCC not connected
**Symptom:** Motors don't respond, even with correct PWM signals
**Cause:** Logic circuitry needs 5V on VCC pin to function
**Fix:** Connect BTS7960 VCC to buck 5V output

### ❌ Mistake: Enable pins floating
**Symptom:** Intermittent motor response, random stops
**Cause:** R_EN and L_EN must be HIGH (5V) or controlled by GPIO
**Fix:** Tie R_EN and L_EN to 5V (or connect to GPIO6/7/10/11)

### ❌ Mistake: Ground loop (ESP32 and motors share ground path)
**Symptom:** ESP32 resets randomly during motor operation
**Cause:** Motor switching noise couples into ESP32 ground
**Fix:** Use star ground topology (see diagram above)

### ❌ Mistake: Reverse polarity on BTS7960 B+ / B-
**Symptom:** BTS7960 overheats immediately, may release magic smoke
**Cause:** Polarity reversed destroys MOSFETs
**Fix:** **Prevention is key!** Use multimeter to verify before connecting battery

### ❌ Mistake: Using charge-only USB cable for ESP32
**Symptom:** Can't flash firmware, no serial output
**Cause:** Some USB cables only have power wires (no data lines)
**Fix:** Use a known-good USB data cable (test on another device first)

### ❌ Mistake: PWM wires too long or unshielded
**Symptom:** Motors behave erratically, EMI affects nearby electronics
**Cause:** Long wires act as antennas, radiate noise
**Fix:** Keep PWM wires short (<20cm), twist pairs together, or use shielded cable

## Motor Polarity / Direction

If motors spin the wrong direction after wiring:

**Option 1: Swap motor wires** (hardware fix)
- Swap M+ and M- on the motor that's backwards

**Option 2: Flip in software** (easier)
Edit `firmware/main/config.h`:
```c
#define MOTOR_LEFT_INVERT   false  // Change to true if left motor backwards
#define MOTOR_RIGHT_INVERT  false  // Change to true if right motor backwards
```

## Adding Current Sense (Advanced)

BTS7960 IS pins output ~0.5V per amp of motor current.

**To monitor current:**
1. Connect BTS7960 L_IS to ESP32 ADC (e.g., GPIO0)
2. Connect BTS7960 R_IS to ESP32 ADC (e.g., GPIO1)
3. Read ADC in firmware:
   ```c
   adc_value = adc1_get_raw(ADC1_CHANNEL_0);
   voltage = (adc_value / 4095.0) * 3.3;  // Assuming 12-bit ADC
   current_amps = voltage / 0.5;  // ~0.5V per amp
   ```
4. Implement overcurrent protection (stop motors if >40A)

## External Encoder Wiring (Future)

If adding quadrature encoders for odometry:

| Encoder Signal | ESP32 Pin | Notes |
|----------------|-----------|-------|
| Left Encoder A | GPIO19 | Interrupt-capable |
| Left Encoder B | GPIO22 | Interrupt-capable |
| Right Encoder A | GPIO23 | Interrupt-capable |
| Right Encoder B | GPIO26 | Interrupt-capable |
| Encoder VCC | 5V | From buck converter |
| Encoder GND | GND | Common ground |

**Note:** ESP32-C5 has limited interrupt-capable pins. Use PCNT (Pulse Counter) peripheral for best performance.

## Troubleshooting Checklist

Before asking for help, verify:

- [ ] Battery voltage is 11-13V (measure with multimeter)
- [ ] Buck converter output is 4.8-5.2V
- [ ] ESP32 5V pin measures 4.8-5.2V
- [ ] BTS7960 VCC (both) measures 4.8-5.2V
- [ ] All GND connections are secure (use continuity test)
- [ ] No shorts between (+) and (-) anywhere (use resistance test)
- [ ] Enable pins (R_EN, L_EN) are HIGH (5V) or controlled by GPIO
- [ ] PWM wires are connected to correct GPIOs (GPIO4/5 for left, GPIO8/9 for right)
- [ ] Motors spin freely by hand (no mechanical binding)
- [ ] Firmware is built for ESP32-C5 target (`idf.py set-target esp32c5`)

## Next Steps

- [Flash firmware](../README.md#flash-firmware)
- [Pair PS4 controller](../README.md#pair-ps4-controller)
- [Tune PWM settings](pwm-configuration.md)
- [Test HTTP API](http-api.md)
