# I2C2 No Activity - Troubleshooting Guide

## Problem: Logic analyzer shows no activity on I2C2 (PB13/PB14)

---

## ✅ Changes Made

### 1. Added Boot-Time I2C2 Test
**File:** `Core/Src/main.c:237-244`

The firmware now sends 5 test transmissions immediately after boot (before LED blinks):
```c
// After 500ms delay, send 5 transmissions:
// 1: 0xAA 0x55 0x00
// 2: 0xAA 0x55 0x01
// 3: 0xAA 0x55 0x02
// 4: 0xAA 0x55 0x03
// 5: 0xAA 0x55 0x04
```

**Timing:** 100ms between each transmission

**This makes testing easier** - you don't need to trigger a commit!

---

## 🔍 Step-by-Step Diagnostic

### Step 1: Flash New Firmware

```bash
cd /home/saolin/STM32_EXP/Nymphes_fv1_programmer/Fv1_programmer
make flash
```

**Expected:** "Verified OK" message

---

### Step 2: Verify Boot Sequence

After flashing, watch the LED:

1. **Power on** → Wait 500ms
2. **I2C2 transmissions** → 5× (500ms total)
3. **LED blinks** → 2 quick blinks
4. **LED slow blinks** → Idle state

**If LED blinks immediately without delay:**
- Boot test code may not be compiled
- Check that `DEBUG_I2C_MIRROR` is defined

---

### Step 3: Hardware Checklist

#### ❌ **Most Common Issue: No Pull-Up Resistors**

**I2C2 requires external pull-ups!**

```
VDD (+3.3V)
  │
  ├─── [4.7kΩ] ───┬─── PB13 (I2C2_SCL)
  │               │
  └─── [4.7kΩ] ───┴─── PB14 (I2C2_SDA)
```

**Without pull-ups:**
- Both lines will be HIGH (pulled up by analyzer input impedance)
- OR both lines will be floating (undefined)
- NO communication will occur

**How to verify:**
- Multimeter: Measure PB13 and PB14 to GND
  - **With pull-ups:** ~3.3V DC
  - **Without pull-ups:** Floating or 0V

#### ❌ **Check Pin Connections**

| STM32 Pin | Function | Logic Analyzer |
|-----------|----------|----------------|
| PB13 | I2C2_SCL | CH0 (or CH_CLK) |
| PB14 | I2C2_SDA | CH1 (or CH_DATA) |
| GND | Ground | GND |

**Verify with multimeter continuity test:**
1. Touch probe to PB13 on STM32
2. Touch probe to logic analyzer CH0 connection
3. Should beep (continuity)

#### ❌ **Pin Conflict Check**

Make sure PB13/PB14 are not used for other functions:

```bash
# Search for other uses of PB13/PB14
grep -r "PB13\|PB14\|GPIO_PIN_13\|GPIO_PIN_14" Core/Src/gpio.c Core/Inc/gpio.h
```

**Expected:** Only I2C2 configuration

---

### Step 4: Logic Analyzer Settings

**Minimum Requirements:**
- **Sample Rate:** ≥ 1 MHz (10× the I2C clock)
- **Trigger:** None (or SDA falling edge)
- **Duration:** 2 seconds
- **Protocol:** I2C (7-bit addressing)

**Common Mistakes:**
- ❌ Sample rate too low (< 500 kHz)
- ❌ Wrong trigger (missing the start condition)
- ❌ Probing wrong pins
- ❌ No ground connection

**PulseView / Sigrok Settings:**
```
Channels:
  D0 = SCL (PB13)
  D1 = SDA (PB14)

Sample rate: 1 MHz or higher
Samples: 2M (2 seconds at 1MHz)

Protocol Decoders:
  I2C
    SCL = D0
    SDA = D1
    Address format = 7-bit
```

---

### Step 5: Expected I2C Traffic

After power-on/reset, you should see **5 identical transactions** like this:

```
Transaction 1 (at ~500ms after boot):
├─ START
├─ Address: 0xC0 (Write to device 0x60)
├─ NACK (no ACK expected - no slave present)
├─ Data: 0xAA
├─ NACK
├─ Data: 0x55
├─ NACK
├─ Data: 0x00  ← Counter value
├─ NACK
└─ STOP

[100ms delay]

Transaction 2:
├─ START
├─ Address: 0xC0
├─ NACK
├─ Data: 0xAA
├─ NACK
├─ Data: 0x55
├─ NACK
├─ Data: 0x01  ← Counter increments
├─ NACK
└─ STOP

... (3 more times with counters 0x02, 0x03, 0x04)
```

**Total time:** ~500ms (5× 100ms)

**I2C Clock:** ~100 kHz (10μs period)

**Each transaction time:** ~500μs

---

### Step 6: Scope/Multimeter Alternative Test

If you don't have a logic analyzer, use an oscilloscope or multimeter:

#### **Oscilloscope:**
1. Connect CH1 to PB13 (SCL)
2. Trigger: Single shot, falling edge, 1V threshold
3. Timebase: 100μs/div
4. Power on device
5. **Expected:** Square wave at ~100kHz for brief periods

#### **Multimeter (DC Voltage):**
1. Set to DC voltage mode
2. Black probe to GND
3. Red probe to PB13
4. Power on device
5. **Expected:**
   - **Idle:** 3.3V (pulled high)
   - **Active:** Brief dips to ~0V during communication

#### **Multimeter (AC Voltage):**
1. Set to AC voltage mode
2. Measure PB13 or PB14
3. **Expected during transmission:** Small AC voltage (< 1V AC)

---

### Step 7: Software Verification

#### **Check Debug Symbol is Defined:**

```bash
# Should show the definition
grep "^#define DEBUG_I2C_MIRROR" Core/Src/main.c Core/Src/ram_disk.c
```

**Expected output:**
```
Core/Src/main.c:#define DEBUG_I2C_MIRROR
Core/Src/ram_disk.c:#define DEBUG_I2C_MIRROR
```

#### **Verify Boot Test Code is Compiled:**

```bash
# Check if test code exists in object file
arm-none-eabi-objdump -d build/main.o | grep -A 10 "test_data"
```

**Should show:** Assembly instructions referencing the test data

#### **Check I2C2 Handle:**

```bash
# Verify hi2c2 is declared
grep "^I2C_HandleTypeDef hi2c2" Core/Src/main.c
```

**Expected:** `I2C_HandleTypeDef hi2c2;`

---

### Step 8: Advanced Debugging - Check Peripheral Registers

If you have OpenOCD or GDB access:

```gdb
# Connect with GDB
arm-none-eabi-gdb build/Fv1_programmer.elf
target extended-remote localhost:3333

# Break at boot test
break main.c:241

# Run and check registers when stopped at transmit
continue

# Check I2C2 peripheral is enabled
# RCC->APB1ENR bit 22 should be 1 (I2C2 clock enabled)
x/1xw 0x40021034
# Expected: bit 22 = 1 (0x00400000 bit set)

# Check I2C2 control register
# I2C2->CR1 at 0x40005800
x/1xw 0x40005800
# Bit 0 (PE) should be 1 (peripheral enabled)

# Check GPIO alternate function for PB13/PB14
# GPIOB->AFR[1] for pins 8-15
x/1xw 0x50000424
# Bits 20-23 (PB13 AF) should be 0x5 (AF5 = I2C2)
# Bits 24-27 (PB14 AF) should be 0x5 (AF5 = I2C2)
```

---

## 🔧 Common Problems & Solutions

| Symptom | Cause | Solution |
|---------|-------|----------|
| **Both lines always HIGH** | No pull-ups installed | Add 4.7kΩ resistors to +3.3V |
| **Both lines floating** | No pull-ups installed | Add 4.7kΩ resistors to +3.3V |
| **Both lines always LOW** | Short circuit | Check wiring, remove short |
| **No change at power-on** | Firmware not flashed | Run `make flash` |
| **Immediate LED blinks** | DEBUG_I2C_MIRROR not defined | Check both .c files have #define |
| **500ms delay but no I2C** | I2C2 clock not enabled | Check RCC->APB1ENR bit 22 |
| **Wrong pins active** | GPIO not configured | Check HAL_I2C_MspInit |

---

## 🎯 Quick Hardware Test

**Bypass firmware - test pins directly:**

### Test 1: GPIO Toggle Test

Modify `main.c` boot section temporarily:

```c
// TEMPORARY TEST - Replace I2C test with GPIO toggle
#ifdef DEBUG_I2C_MIRROR
HAL_Delay(500);

// Configure PB13 as output
GPIO_InitTypeDef GPIO_Test;
GPIO_Test.Pin = GPIO_PIN_13;
GPIO_Test.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_Test.Pull = GPIO_NOPULL;
GPIO_Test.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
HAL_GPIO_Init(GPIOB, &GPIO_Test);

// Toggle PB13 10 times
for (int i = 0; i < 10; i++) {
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_Delay(100);
}

// Re-configure as I2C (call MX_I2C2_Init again if needed)
#endif
```

**Expected on logic analyzer:** Square wave at 5Hz (100ms period)

**If this works:** GPIO pins are functional, issue is with I2C peripheral

**If this doesn't work:** Pin connection problem

---

## 📋 Final Checklist

Before asking for more help, verify:

- [ ] **Pull-up resistors installed** on PB13 and PB14 (4.7kΩ to +3.3V)
- [ ] **Firmware rebuilt** after adding DEBUG_I2C_MIRROR (`make clean && make`)
- [ ] **Firmware flashed** successfully (`make flash` shows "Verified OK")
- [ ] **Logic analyzer** connected to correct pins (PB13=CH0, PB14=CH1, GND=GND)
- [ ] **Logic analyzer** sample rate ≥ 1 MHz
- [ ] **Logic analyzer** capturing during boot (trigger before power-on)
- [ ] **LED sequence** correct (500ms delay, 2 blinks, slow blink)
- [ ] **Pin voltage** measures ~3.3V with multimeter (confirms pull-ups)

---

## 🆘 If Still No Activity

**Most likely causes (in order of probability):**

1. **80% - No pull-up resistors**
   - **Solution:** Add 4.7kΩ from PB13 to +3.3V and PB14 to +3.3V

2. **10% - Wrong pins probed**
   - **Solution:** Double-check pin mapping, try swapping CH0/CH1

3. **5% - I2C2 clock not enabled**
   - **Solution:** Check `__HAL_RCC_I2C2_CLK_ENABLE()` is called

4. **3% - GPIO not configured for I2C**
   - **Solution:** Verify `HAL_I2C_MspInit` configures PB13/PB14 as AF5

5. **2% - Hardware damage**
   - **Solution:** Try GPIO toggle test above

**Report back with:**
1. Multimeter voltage reading on PB13 and PB14
2. LED boot sequence timing (does it have 500ms delay?)
3. Logic analyzer screenshot
4. Confirmation pull-ups are installed
