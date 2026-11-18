# Button Interaction - Complete Code Reference

## Overview

The button uses a **debouncing state machine** that runs in the SysTick interrupt (1ms intervals) and is checked in the main loop to trigger EEPROM commits.

---

## Complete Button Flow

```
┌────────────────────────────────────────────────────────────────────┐
│ HARDWARE: Button connected to BUTTON_Pin (PB11)                   │
│ Configuration: Active LOW with internal pull-up                   │
│ - Pressed: Pin reads LOW (GPIO_PIN_RESET)                         │
│ - Released: Pin reads HIGH (GPIO_PIN_SET)                         │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ INTERRUPT: SysTick (fires every 1ms)                              │
│ File: Core/Src/stm32l0xx_it.c:133                                 │
│                                                                    │
│ void SysTick_Handler(void)                                        │
│ {                                                                  │
│     HAL_IncTick();                                                 │
│     GPIO_Update();  // ← Called every 1ms                         │
│ }                                                                  │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ DEBOUNCING: GPIO_Update() runs state machine                      │
│ File: Core/Src/gpio.c:121                                         │
│                                                                    │
│ Static state:                                                      │
│   - raw_state: Immediate pin reading                              │
│   - debounce_timer: Counts stable time (50ms threshold)           │
│   - current_state: Debounced button state                         │
│   - previous_state: For edge detection                            │
│   - press_timer: For long press (2000ms threshold)                │
│                                                                    │
│ Logic:                                                             │
│   1. Read raw pin state                                           │
│   2. If changed: reset timer                                      │
│   3. If stable for 50ms: update current_state                     │
│   4. Track previous_state for edge detection                      │
│   5. If held for 2000ms: set long_press_flag                      │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ MAIN LOOP: Checks button every 10ms                               │
│ File: Core/Src/main.c:239-256                                     │
│                                                                    │
│ while (1) {                                                        │
│     CheckRamDiskStatus();  // Update app_state                    │
│     HandleButtonPress();    // ← Check button                     │
│     UpdateLED();            // Update LED pattern                 │
│     HAL_Delay(10);          // Loop delay                         │
│ }                                                                  │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ BUTTON CHECK: HandleButtonPress()                                 │
│ File: Core/Src/main.c:159                                         │
│                                                                    │
│ void HandleButtonPress(void)                                      │
│ {                                                                  │
│     if (Button_WasPressed()) {  // ← Edge detection               │
│         if (app_state == STATE_PENDING) {  // ← State check       │
│             app_state = STATE_COMMITTING;                         │
│             bool success = RamDisk_CommitToEEPROM();  // ← Commit │
│                                                                    │
│             if (success) {                                         │
│                 app_state = STATE_SUCCESS;                        │
│                 state_timer = HAL_GetTick();                      │
│             } else {                                               │
│                 app_state = STATE_ERROR;                          │
│                 state_timer = HAL_GetTick();                      │
│             }                                                      │
│         }                                                          │
│     }                                                              │
│ }                                                                  │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ EDGE DETECTION: Button_WasPressed()                               │
│ File: Core/Src/gpio.c:86                                          │
│                                                                    │
│ bool Button_WasPressed(void)                                      │
│ {                                                                  │
│     // Returns true ONCE per button press (rising edge)           │
│     if (button_state.current_state &&                             │
│         !button_state.previous_state) {                           │
│         return true;  // ← Pressed transition detected            │
│     }                                                              │
│     return false;                                                 │
│ }                                                                  │
└────────────────────┬───────────────────────────────────────────────┘
                     │
                     ▼
┌────────────────────────────────────────────────────────────────────┐
│ COMMIT: RamDisk_CommitToEEPROM()                                  │
│ File: Core/Src/ram_disk.c:297                                     │
│                                                                    │
│ 1. Find file in cached directory entry                            │
│ 2. Parse FAT12 cluster chain                                      │
│ 3. Extract file data from RAM buffer                              │
│ 4. Write directly to EEPROM via I2C                               │
│ 5. Pad remaining space with 0xFF                                  │
│ 6. Return success/failure                                         │
└────────────────────────────────────────────────────────────────────┘
```

---

## Code Locations

### 1. **Button State Structure** - `Core/Src/gpio.c:11-18`

```c
static struct {
    bool current_state;      // Current debounced button state
    bool previous_state;     // Previous debounced state for edge detection
    bool raw_state;          // Raw button reading
    uint32_t debounce_timer; // Debounce timer counter (in ms)
    uint32_t press_timer;    // Press duration timer (in ms)
    bool long_press_flag;    // Flag indicating long press detected
} button_state = {0};
```

### 2. **Debouncing Configuration** - `Core/Inc/gpio.h:42-43`

```c
#define DEBOUNCE_TIME_MS    50      // Button debounce time in milliseconds
#define LONG_PRESS_TIME_MS  2000    // Long press detection time in milliseconds
```

### 3. **GPIO Update (Debouncing)** - `Core/Src/gpio.c:121`

```c
void GPIO_Update(void)
{
    // Read raw button state (active LOW with pull-up)
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
    bool current_raw = (pin_state == GPIO_PIN_RESET);

    // Debouncing logic
    if (current_raw != button_state.raw_state) {
        // Button state changed, reset debounce timer
        button_state.raw_state = current_raw;
        button_state.debounce_timer = 0;
    } else {
        // Button state stable, increment timer
        button_state.debounce_timer++;

        // Check if debounce time has elapsed
        if (button_state.debounce_timer >= DEBOUNCE_TIME_MS) {
            // Update previous state before changing current state
            button_state.previous_state = button_state.current_state;
            button_state.current_state = button_state.raw_state;

            // Reset debounce timer to prevent overflow
            button_state.debounce_timer = DEBOUNCE_TIME_MS;
        }
    }

    // Long press detection (not currently used)
    if (button_state.current_state) {
        button_state.press_timer++;
        if (button_state.press_timer >= LONG_PRESS_TIME_MS) {
            button_state.long_press_flag = true;
            button_state.press_timer = LONG_PRESS_TIME_MS;
        }
    } else {
        button_state.press_timer = 0;
        button_state.long_press_flag = false;
    }
}
```

### 4. **SysTick Interrupt** - `Core/Src/stm32l0xx_it.c:126`

```c
void SysTick_Handler(void)
{
    HAL_IncTick();
    GPIO_Update();  // Update button debouncing (called every 1ms)
}
```

### 5. **Button API Functions** - `Core/Src/gpio.c`

```c
// Check if button is currently pressed (level detection)
bool Button_IsPressed(void)
{
    return button_state.current_state;
}

// Check if button was JUST pressed (edge detection) ← Used by firmware
bool Button_WasPressed(void)
{
    if (button_state.current_state && !button_state.previous_state) {
        return true;  // Rising edge detected
    }
    return false;
}

// Check if button held for 2 seconds (long press)
bool Button_IsLongPress(void)
{
    return button_state.long_press_flag;
}
```

### 6. **Main Loop Button Handler** - `Core/Src/main.c:159`

```c
void HandleButtonPress(void)
{
    if (Button_WasPressed()) {  // Edge-triggered, returns true once per press
        if (app_state == STATE_PENDING) {  // Only works in PENDING state
            // Start commit
            app_state = STATE_COMMITTING;

            // Perform commit (this may take a few hundred ms)
            bool success = RamDisk_CommitToEEPROM();

            if (success) {
                app_state = STATE_SUCCESS;
                state_timer = HAL_GetTick();
            } else {
                app_state = STATE_ERROR;
                state_timer = HAL_GetTick();
            }
        }
    }
}
```

### 7. **Main Loop** - `Core/Src/main.c:239`

```c
while (1)
{
    // Update state based on RamDisk
    CheckRamDiskStatus();

    // Handle button press
    HandleButtonPress();  // ← Called every 10ms

    // Update LED based on state
    UpdateLED();

    // Small delay to prevent tight loop
    HAL_Delay(10);
}
```

---

## Button Timing Diagram

```
Time:     0ms    50ms   100ms  150ms  200ms  250ms  300ms
         │       │      │      │      │      │      │
Pin:     HIGH────┐                             ┌────HIGH
                 └─────────────────────────────┘
                 Button pressed          Button released

raw_state:   0   ┌──────────────────────────────┐   0
                 1                              1

debounce_timer: 0,1,2...50  (stays at 50)     0,1,2...50

current_state:      0      ┌──────────────────┐    0
                          1 (after 50ms)      1

previous_state:  0         0  ┌───────────────┐   1
                               1               1

Button_WasPressed():        TRUE (once)        FALSE
                            ↑
                            Main loop detects here
```

---

## Critical Requirements

### ✅ For Button to Work:

1. **Write-protect switch UNLOCKED**
   - Allows USB writes → RAM buffer

2. **File written to device**
   - OS writes file → RamDisk_WriteSector() called
   - Data sectors (5-12) written → `current_status = RAMDISK_STATUS_PENDING`

3. **CheckRamDiskStatus() updates app_state**
   - Reads `RAMDISK_STATUS_PENDING`
   - Sets `app_state = STATE_PENDING`

4. **LED shows FAST blink**
   - UpdateLED() reads `app_state == STATE_PENDING`
   - Blinks LED at 200ms intervals

5. **Button press detected**
   - GPIO_Update() debounces (50ms)
   - Button_WasPressed() returns true (edge)
   - HandleButtonPress() checks `app_state == STATE_PENDING` ✅
   - Calls RamDisk_CommitToEEPROM()

### ❌ Button Does Nothing If:

- `app_state != STATE_PENDING`
- No file written (still in STATE_IDLE)
- Write-protect switch locked (no writes = no PENDING state)
- Button not connected to PB11
- GPIO_Update() not running (SysTick issue)

---

## Debugging Commands

### Check if button hardware works:
```bash
# This would require OpenOCD or ST-Link utilities
# Read GPIOB IDR register to see button state
```

### Check app_state in firmware:
Add debug print in main.c:
```c
// In main loop, add:
static AppState last_state = STATE_IDLE;
if (app_state != last_state) {
    // State changed - could toggle LED2 or use SWO printf
    last_state = app_state;
}
```

### Verify button debouncing:
The button requires:
- Press duration > 50ms to debounce
- Main loop polls every 10ms
- Edge detection triggers once per press

---

## Summary

**Button interaction happens in 4 layers:**

1. **Hardware Layer**: Physical button on PB11 (active LOW)
2. **Interrupt Layer**: SysTick calls GPIO_Update() every 1ms for debouncing
3. **API Layer**: Button_WasPressed() provides edge detection
4. **Application Layer**: HandleButtonPress() checks state and triggers commit

**The key requirement**: `app_state` must be `STATE_PENDING` for button to work!
