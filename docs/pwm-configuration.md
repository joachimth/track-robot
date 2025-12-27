# PWM Configuration Guide

## Overview

Motor control performance depends heavily on PWM (Pulse Width Modulation) frequency. This guide explains the tradeoffs and how to optimize for your setup.

## Current Configuration

**Default: 20kHz (20,000 Hz)**

This is chosen as the **best balance** for tracked robot applications:
- ✅ Above audible range (humans hear up to ~16kHz) - motors run silently
- ✅ Well within BTS7960 spec (max 25kHz)
- ✅ Smooth motor control with minimal torque ripple
- ✅ Low EMI (electromagnetic interference)
- ✅ ESP32-C5 handles easily at 240MHz

## BTS7960 H-Bridge Specifications

**From datasheet:**
- **Maximum PWM frequency**: 25kHz
- **Recommended range**: 10kHz - 25kHz
- **Switching time**: ~200ns (rise/fall)
- **Dead time**: Integrated (no external dead-time needed)

**Observations:**
- Below 10kHz: Audible whine, higher torque ripple
- 10-20kHz: Good performance, silent operation
- 20-25kHz: Optimal (our choice)
- Above 25kHz: Not recommended (switching losses increase, driver may overheat)

## Frequency Tradeoffs

| Frequency | Pros | Cons | Use Case |
|-----------|------|------|----------|
| **1 kHz** | Simple, low EMI | **Loud whine**, poor control | ❌ Not recommended |
| **5 kHz** | Low switching losses | Audible whine, visible stuttering | ❌ Not recommended |
| **10 kHz** | Silent, good efficiency | Slight torque ripple at low speeds | ⚠️ Acceptable minimum |
| **20 kHz** | **Silent, smooth, optimal** | Slightly higher heat in driver | ✅ **Default choice** |
| **25 kHz** | Maximum smoothness | Near BTS7960 limit, more heat | ⚠️ Advanced users only |
| **>25 kHz** | N/A | Driver not rated, may fail | ❌ Unsafe |

## How to Change PWM Frequency

### Option 1: Edit config.h (Recommended)

Edit `firmware/main/config.h`:

```c
// PWM Configuration
#define MOTOR_PWM_FREQUENCY_HZ  20000  // Change this value
```

**Examples:**
- Conservative (guaranteed silent): `15000` (15kHz)
- Aggressive (max smoothness): `25000` (25kHz)
- Testing/debug: `10000` (10kHz, easier to measure with oscilloscope)

### Option 2: Runtime Tuning (Advanced)

Modify `motor_bts7960_init()` in `motor_bts7960.c`:

```c
ledc_timer_config_t timer_config = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,  // 1024 steps
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 20000,  // <-- Change here for testing
    .clk_cfg = LEDC_AUTO_CLK
};
```

Rebuild and flash: `idf.py build flash`

## Resolution vs Frequency

ESP32-C5 LEDC (LED PWM Controller) has limited clock speed. Higher frequency = lower resolution.

**Formula:**
```
Max Frequency = Clock Speed / (2^Resolution)

For ESP32-C5 @ 80MHz APB clock:
- 10-bit (1024 steps): Max ~78kHz
- 8-bit (256 steps):   Max ~312kHz
```

**Current config:**
- Resolution: 10-bit (1024 steps) - `LEDC_TIMER_10_BIT` in code
- Frequency: 20kHz
- Effective speed steps: 0-1023 (excellent granularity)

**If you increase frequency to 25kHz:**
- Still fits within 10-bit @ 80MHz (✓)
- 1024 speed steps maintained

**If you try 40kHz:**
- Must drop to 9-bit (512 steps) or accept jitter
- Not recommended (outside BTS7960 spec anyway)

## Tuning for Your Motors

### Windshield Wiper Motors (Topran 108 792)

**Characteristics:**
- 12V rated
- High torque, low speed (~50-100 RPM no-load)
- Inductive load (large back-EMF when stopping)
- Internal gearing (mechanical inertia)

**Recommended PWM settings:**
- Frequency: **20kHz** (default) works excellently
- Resolution: 10-bit (fine speed control)
- Slew rate: `MOTOR_RAMP_RATE = 5.0` (5% duty per 10ms) prevents gear shock

### If Using Different Motors

**High-speed brushless (e.g., RC car motors):**
- May benefit from 25kHz (smoother at high RPM)
- Reduce slew rate to 10.0 (faster response)

**Low-speed geared DC (e.g., drill motors):**
- 15kHz sufficient (high inertia smooths naturally)
- Increase slew rate to 2.0 (slower ramp, protect gears)

**Stepper motors:**
- This firmware is NOT designed for steppers (use dedicated stepper driver)

## Measuring PWM Output

**Tools:**
- **Oscilloscope**: Ideal (probe GPIO4-11 to see PWM waveform)
- **Logic analyzer**: Good (can decode duty cycle)
- **Multimeter (AC mode)**: Rough estimate only

**Expected waveform @ 20kHz, 50% duty:**
- Period: 50µs (1/20kHz)
- High time: 25µs
- Low time: 25µs
- Voltage: 3.3V high, 0V low (ESP32 GPIO)

**BTS7960 input (RPWM/LPWM):**
- Logic high threshold: >2.0V (3.3V from ESP32 = ✓)
- Logic low threshold: <0.8V (0V from ESP32 = ✓)
- No level shifter needed

## Common Issues & Solutions

### Issue: Motors make high-pitched whine

**Cause:** PWM frequency too low (in audible range)

**Solution:**
- Increase to 20kHz: `#define MOTOR_PWM_FREQUENCY_HZ 20000`
- If still audible, try 25kHz

### Issue: Motors stutter or vibrate at low speeds

**Cause:** PWM frequency too high, or insufficient resolution

**Solution:**
- Use 20kHz with 10-bit resolution (default)
- Increase deadzone: `#define STICK_DEADZONE 0.08` (8%)
- Check for loose mechanical connections

### Issue: BTS7960 overheats

**Cause:** Switching losses at high frequency, or excessive current

**Solution:**
- Reduce frequency to 15kHz: `#define MOTOR_PWM_FREQUENCY_HZ 15000`
- Add heatsink to BTS7960 (recommended anyway)
- Check motor current (should be <43A per driver)
- Verify 12V supply is stable (voltage sag causes overcurrent)

### Issue: ESP32 crashes or reboots during motor control

**Cause:** Electrical noise from motors coupling into ESP32 power/ground

**Solution:**
- **Critical:** Separate power grounds! Use star ground topology:
  ```
  Battery (-)
    ├─→ BTS7960 GND (thick wire)
    └─→ Buck GND → ESP32 GND (thin wire, single point connection)
  ```
- Add 100µF capacitor across BTS7960 motor terminals (suppresses back-EMF)
- Add 100nF ceramic cap across ESP32 5V input (filters high-freq noise)
- Twist PWM signal wires to reduce radiated EMI

### Issue: Motors don't respond smoothly to stick input

**Cause:** Slew rate limiting too aggressive, or control loop too slow

**Solution:**
- Reduce ramp rate: `#define MOTOR_RAMP_RATE 8.0` (faster response)
- Increase control loop frequency in `main.c`:
  ```c
  vTaskDelay(pdMS_TO_TICKS(10));  // Change from 20ms to 10ms
  ```
- Check expo curve: `#define STICK_EXPO 1.5` (1.0 = linear, 2.0 = more expo)

## Advanced: Current Sensing

BTS7960 has built-in current sense outputs (IS pin):
- ~0.5V per amp
- Can be read with ESP32 ADC

**To enable (future enhancement):**
1. Connect BTS7960 L_IS and R_IS pins to ESP32 ADC channels (e.g., GPIO0, GPIO1)
2. Read ADC in `motor_bts7960.c`
3. Implement overcurrent protection:
   ```c
   if (current_amps > 40.0) {
       // Reduce PWM or emergency stop
   }
   ```

## PWM Frequency Selection Flowchart

```
Start
  ↓
Are motors audibly whining?
  ├─ YES → Try 20kHz → Still whining? → Try 25kHz → Still? → Check mechanical issues
  └─ NO → Continue
  ↓
Is motor control smooth and responsive?
  ├─ YES → **You're done! (20kHz is optimal)**
  └─ NO → Continue
  ↓
Do motors stutter at low speeds?
  ├─ YES → Check slew rate (may be too fast)
  └─ NO → Check deadzone and expo
  ↓
Does BTS7960 overheat during use?
  ├─ YES → Try 15kHz, add heatsink, check current
  └─ NO → Continue
  ↓
Is there electrical noise (ESP32 crashes)?
  ├─ YES → Fix grounding! (see above)
  └─ NO → You're done!
```

## Recommended Settings Summary

**For Topran 108 792 wiper motors (this project's default):**
```c
#define MOTOR_PWM_FREQUENCY_HZ  20000    // 20kHz (silent, optimal)
#define MOTOR_PWM_RESOLUTION    10       // 10-bit (1024 steps)
#define MOTOR_RAMP_RATE        5.0      // 5% per 10ms (smooth)
#define MOTOR_MAX_DUTY         1.0      // 100% (full power)
#define MOTOR_MIN_DUTY         0.05     // 5% (stall threshold)
```

**Testing/troubleshooting:**
```c
#define MOTOR_PWM_FREQUENCY_HZ  10000    // Lower for scope measurement
#define MOTOR_RAMP_RATE        0.0      // Disable ramping for testing
```

**Maximum performance (advanced users):**
```c
#define MOTOR_PWM_FREQUENCY_HZ  25000    // Max BTS7960 can handle
#define MOTOR_RAMP_RATE        10.0     // Faster response
```

## References

- BTS7960 Datasheet: [Infineon BTS7960B](https://www.infineon.com/dgdl/Infineon-BTS7960-DataSheet-v01_00-EN.pdf?fileId=db3a30431936bc4b0119539d31fa23d1)
- ESP32-C5 LEDC: [ESP-IDF LEDC Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/api-reference/peripherals/ledc.html)
- PWM frequency selection: [TI Motor Drive PWM App Note](http://www.ti.com/lit/an/slva887/slva887.pdf)
