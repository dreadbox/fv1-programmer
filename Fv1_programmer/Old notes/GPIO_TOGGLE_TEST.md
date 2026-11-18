# GPIO Toggle Test - Pin Verification

## Problem: SDA goes LOW but SCL doesn't move

This suggests the I2C peripheral might be configured incorrectly, or the SCL pin isn't working.

**New test:** Bypass I2C peripheral entirely and toggle pins as GPIO.

---

## 🔧 What The New Test Does

After power-on, the firmware will:

1. **Wait 500ms**
2. **Disable I2C2 peripheral**
3. **Configure PB13 and PB14 as GPIO outputs**
4. **Test 1 (1 second):** Toggle both pins together 10 times at 10Hz
5. **Wait 200ms**
6. **Test 2 (1 second):** Toggle pins alternately 10 times at 10Hz
7. **Re-initialize I2C2**
8. **LED blinks** twice

**Total test time:** ~2.7 seconds before LED blinks

---

## 📊 Expected Logic Analyzer Output

### **Test 1: Both Pins Together (1 second)**

```
PB13 (SCL): ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
              └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘

PB14 (SDA): ──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──
              └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
```

**Both pins should be IDENTICAL** - toggling in sync at 10Hz (50ms high, 50ms low)

---

### **Test 2: Alternating (1 second)**

```
PB13 (SCL): ──┐     ┌──┐     ┌──┐     ┌──┐     ┌──┐     ┌──┐     ┌──┐
              └─────┘  └─────┘  └─────┘  └─────┘  └─────┘  └─────┘

PB14 (SDA):     ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐  ┌
              ──┘     └──┘     └──┘     └──┘     └──┘     └──┘     └──┘
```

**Pins should be OPPOSITE** - when SCL is HIGH, SDA is LOW and vice versa

---

## 🎯 Diagnostic Results

### ✅ **PASS - Both Tests Work**

**What you see:**
- Test 1: Both pins toggle together in perfect sync
- Test 2: Pins alternate (inverse of each other)

**Conclusion:**
- ✅ PB13 hardware is working
- ✅ PB14 hardware is working
- ✅ GPIO configuration is correct
- ✅ Logic analyzer connections are correct
- ❌ **Problem is with I2C2 peripheral configuration**

**Next step:** Check I2C2 initialization (timing, clock, alternate function)

---

### ⚠️ **PARTIAL - Only Test 1 Works, Test 2 Shows Both Moving**

**What you see:**
- Test 1: Both pins toggle together ✅
- Test 2: Both pins still toggle together (not alternating) ❌

**Conclusion:**
- Firmware bug in toggle code (unlikely)
- Check if HAL_GPIO_WritePin is being optimized out

**Next step:** Add delay between pin writes in Test 2

---

### ❌ **FAIL - Only PB14 (SDA) Toggles**

**What you see:**
- Test 1: Only SDA toggles, SCL stays HIGH or LOW
- Test 2: Only SDA toggles

**Conclusion:**
- ✅ PB14 (SDA) hardware works
- ❌ **PB13 (SCL) pin problem**

**Possible causes:**
1. **PB13 is used by another peripheral** (check for conflicts)
2. **PB13 hardware damage** (unlikely but possible)
3. **PB13 not connected to logic analyzer** (check wiring)
4. **GPIO clock not enabled for GPIOB** (very unlikely - PB14 works)

**Next step:**
- Check with multimeter if PB13 voltage changes during test
- Verify logic analyzer probe is connected to PB13

---

### ❌ **FAIL - Neither Pin Toggles**

**What you see:**
- Both pins stay constant (HIGH or LOW)
- No activity during test period

**Conclusion:**
- ❌ **Firmware not flashed** OR
- ❌ **DEBUG_I2C_MIRROR not defined** OR
- ❌ **Logic analyzer not capturing during boot**

**Next step:**
- Verify LED has ~3 second delay before blinking
- Re-flash: `make flash`
- Check DEBUG_I2C_MIRROR is defined

---

### ❌ **FAIL - Only PB13 (SCL) Toggles**

**What you see:**
- Test 1: Only SCL toggles, SDA stays HIGH or LOW
- Test 2: Only SCL toggles

**Conclusion:**
- ✅ PB13 (SCL) hardware works
- ❌ **PB14 (SDA) pin problem**

**BUT THIS CONTRADICTS YOUR INITIAL OBSERVATION!**

You said "SDA goes low" in the I2C test, so SDA definitely works.

**This would mean:**
- Your logic analyzer channels are **swapped**
- CH0 is actually SDA (PB14)
- CH1 is actually SCL (PB13)

**Next step:** Swap logic analyzer channels and retry

---

## 🔬 Detailed Analysis Based On Your Symptoms

### **You reported: "Only SDA goes LOW"**

This means in the I2C test:
1. **START condition sent** (SDA goes LOW while SCL HIGH) ✅
2. **SCL never clocks** ❌

**With GPIO test, you should see one of these:**

#### **Scenario A: SCL Pin Works in GPIO Mode**
- **Test 1:** Both pins toggle
- **Test 2:** Pins alternate
- **Conclusion:** I2C peripheral SCL clock generation is broken

#### **Scenario B: SCL Pin Doesn't Work in GPIO Mode**
- **Test 1:** Only SDA toggles
- **Test 2:** Only SDA toggles
- **Conclusion:** PB13 hardware problem or not connected

#### **Scenario C: Channels Swapped on Logic Analyzer**
- **Test 1:** Only "SCL" toggles (but it's actually SDA)
- **Test 2:** Only "SCL" toggles
- **Conclusion:** Swap CH0 and CH1

---

## 🔍 What To Check Next

### **1. Flash New Firmware**

```bash
make flash
```

### **2. Setup Logic Analyzer**

**Settings:**
- Sample rate: 1 MHz or higher
- Capture: **Start BEFORE power-on**, capture for 5 seconds
- Channels: CH0 = PB13, CH1 = PB14

### **3. Power Cycle**

- Power off device
- Start logic analyzer capture
- Power on device
- Wait for LED to blink

### **4. Analyze Capture**

**Timeline:**
```
0ms ─────────────────► Power on
500ms ───────────────► Test 1 starts (both pins together)
1500ms ──────────────► Gap (200ms)
1700ms ──────────────► Test 2 starts (alternating)
2700ms ──────────────► LED blinks twice
3100ms ──────────────► Slow blink starts
```

### **5. Report Results**

Take screenshots and report:

**Question 1:** Do you see Test 1 (both pins toggling together)?
- [ ] Yes, both pins toggle
- [ ] Only CH0 toggles
- [ ] Only CH1 toggles
- [ ] Neither toggles

**Question 2:** Do you see Test 2 (pins alternating)?
- [ ] Yes, pins alternate (opposite states)
- [ ] No, they still toggle together
- [ ] Only one pin toggles
- [ ] Neither toggles

**Question 3:** Timing check
- [ ] ~3 second delay before LED blinks
- [ ] LED blinks immediately (no delay)

**Question 4:** Multimeter test
- Measure DC voltage on PB13 during Test 1
  - Expected: Should vary between ~0V and ~3.3V
  - Actual: _____ V

- Measure DC voltage on PB14 during Test 1
  - Expected: Should vary between ~0V and ~3.3V
  - Actual: _____ V

---

## 🛠️ Quick Fixes Based On Results

| Test 1 Result | Test 2 Result | Diagnosis | Fix |
|---------------|---------------|-----------|-----|
| Both toggle | Both alternate | ✅ Pins OK | I2C timing issue - check I2C2 init |
| Both toggle | Both together | ⚠️ Code issue | Check HAL version |
| Only SDA | Only SDA | ❌ SCL broken | Check PB13 connection |
| Only SCL | Only SCL | ⚠️ Swapped | Swap logic analyzer CH0/CH1 |
| Neither | Neither | ❌ Not running | Reflash firmware |

---

## 📋 Action Items

1. **Flash** new firmware with GPIO toggle test
2. **Capture** logic analyzer during boot (start before power-on)
3. **Check** if both pins toggle during Test 1
4. **Report** which scenario matches your observation
5. **Measure** voltages with multimeter if pins don't toggle

This test will definitively tell us if it's a **hardware issue** or **I2C peripheral configuration issue**.
