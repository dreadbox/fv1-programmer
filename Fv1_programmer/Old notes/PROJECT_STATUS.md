# FV1 EEPROM Programmer - Project Status Document

**Date:** 2025-12-30
**MCU:** STM32L053R8T6
**Project Status:** Phase 1, 2 & 3 Complete (USB Testing Pending)

---

## Project Overview

An STM32-based device for programming the EEPROM inside the FV1 DSP processor by Spin Semiconductor. The device features:

- USB Mass Storage Device (MSD) - appears as removable drive
- Drag-and-drop FV1 program loading
- Physical button to trigger FV1 EEPROM programming
- Switch for write protection
- Internal 24LC32AT EEPROM (4KB) as storage backend
- I2C interface to program external FV1 EEPROM

---

## Hardware Specifications

### MCU
- **Model:** STM32L053R8T6
- **Flash:** 64KB
- **RAM:** 8KB
- **Clock:** 32MHz (PLL from HSI)
- **USB Clock:** 48MHz (HSI48)

### Pin Assignments

| Function | Pin(s) | Configuration | Status |
|----------|--------|---------------|--------|
| **I2C1 - Internal EEPROM** | PB8 (SCL), PB9 (SDA) | I2C, 100kHz | ✅ Working |
| **I2C2 - FV1 EEPROM** | PB13 (SCL), PB14 (SDA) | I2C, 100kHz | ⏳ Not tested |
| **USB** | PA11 (D-), PA12 (D+) | USB 2.0 Full Speed | ⏳ Configured, not tested |
| **Button** | PB11 | GPIO Input, Pull-up, Active LOW | ✅ Working |
| **Switch** | PA2 | GPIO Input, Pull-up | ✅ Working |
| **LED1** | PA0 | GPIO Output, Push-Pull | ✅ Working |
| **LED2** | PA1 | GPIO Output, Push-Pull | ❌ **Hardware Issue** |

### External Components

**Internal EEPROM (24LC32AT):**
- Capacity: 4096 bytes (32Kbit)
- I2C Address: 0x50 (7-bit)
- Page Size: 32 bytes
- Write Cycle Time: 5ms max
- Interface: I2C1 (PB8/PB9)
- Status: ✅ Tested and working

**FV1 EEPROM:**
- Capacity: 4096 bytes (matches 24LC32AT)
- Expected to support 8 FV1 programs (512 bytes each)
- Interface: I2C2 (PB13/PB14)
- Status: ⏳ Not yet tested

---

## Completed Work

### Phase 1: GPIO Setup and Hardware Test ✅

**Files Created:**
- `Core/Inc/gpio.h` - GPIO driver header
- `Core/Src/gpio.c` - GPIO driver implementation

**Features Implemented:**
- LED control functions (LED1_On/Off/Toggle)
- Button debouncing (50ms software debounce via SysTick)
- Switch state reading
- Long press detection (2 seconds)
- Edge detection for button press events

**Testing Results:**
- ✅ LED1 working correctly
- ✅ Button debouncing verified
- ✅ Switch reading verified
- ✅ Long press detection verified
- ❌ LED2 hardware issue (PA1 not responding)

**Known Issues:**
- **LED2 (PA1) Hardware Problem:** LED does not light up when GPIO is driven HIGH. Possible causes:
  - LED not connected or incorrectly soldered
  - Wrong polarity
  - Pin conflict or damage
  - **Workaround:** Using only LED1 for status indication

---

### Phase 2: Internal EEPROM Driver ✅

**Files Created:**
- `Core/Inc/eeprom_24lc32.h` - EEPROM driver header
- `Core/Src/eeprom_24lc32.c` - EEPROM driver implementation

**Features Implemented:**

```c
// Main API Functions
EEPROM_Status EEPROM_Init(void);
EEPROM_Status EEPROM_ReadByte(uint16_t addr, uint8_t *data);
EEPROM_Status EEPROM_WriteByte(uint16_t addr, uint8_t data);
EEPROM_Status EEPROM_ReadBuffer(uint16_t addr, uint8_t *buffer, uint16_t size);
EEPROM_Status EEPROM_WriteBuffer(uint16_t addr, uint8_t *buffer, uint16_t size);
EEPROM_Status EEPROM_EraseFull(void);
bool EEPROM_IsReady(void);
EEPROM_Status EEPROM_WaitReady(uint32_t timeout_ms);
```

**Key Features:**
- Automatic page boundary handling (32-byte pages)
- ACK polling for write completion (instead of fixed delays)
- Full 4KB address range support (0x0000 - 0x0FFF)
- Parameter validation
- Timeout protection on all I2C operations

**Testing Results:**
- ✅ EEPROM detected on I2C bus (address 0x50)
- ✅ Single byte write/read at address 0x0000 (value 0xAA)
- ✅ Single byte write/read at address 0x0FFF (value 0x55)
- ✅ 64-byte buffer write/read across page boundary (0x0010-0x004F)
- ✅ Page boundary handling verified
- ✅ ACK polling verified

---

## Project Structure

```
Fv1_programmer/
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── gpio.h                 ← Phase 1: GPIO driver header
│   │   ├── eeprom_24lc32.h       ← Phase 2: EEPROM driver header
│   │   ├── stm32l0xx_hal_conf.h
│   │   └── stm32l0xx_it.h
│   └── Src/
│       ├── main.c                 ← Main application & test routines
│       ├── gpio.c                 ← Phase 1: GPIO driver
│       ├── eeprom_24lc32.c       ← Phase 2: EEPROM driver
│       ├── stm32l0xx_it.c        ← Interrupt handlers (SysTick for debouncing)
│       ├── stm32l0xx_hal_msp.c   ← HAL MSP init (I2C, GPIO clocks)
│       └── system_stm32l0xx.c
├── USB_DEVICE/
│   ├── App/
│   │   ├── usb_device.c
│   │   ├── usbd_desc.c
│   │   └── usbd_storage_if.c     ← USB MSD storage callbacks (Phase 3)
│   └── Target/
│       └── usbd_conf.c
├── FATFS/
│   ├── App/
│   │   └── fatfs.c
│   └── Target/
│       └── user_diskio.c         ← Disk I/O layer (Phase 3)
├── Drivers/                      ← STM32 HAL drivers
├── Middlewares/                  ← USB & FatFs middleware
├── Makefile                      ← Build configuration
├── STM32L053XX_FLASH.ld         ← Linker script
└── PROJECT_STATUS.md            ← This document
```

---

## Build Instructions

### Prerequisites
- `arm-none-eabi-gcc` toolchain installed
- `make` utility

### Build Commands

```bash
# Clean build
make clean

# Build project
make -j4

# Output files
build/Fv1_programmer.elf    # ELF file for debugging
build/Fv1_programmer.bin    # Binary for flashing
build/Fv1_programmer.hex    # Hex format for flashing
build/Fv1_programmer.map    # Memory map
```

### Flash Size
- **Phase 2:** ~23KB flash, 312 bytes data, 4KB BSS
- **Phase 3:** 23.4KB flash, 312 bytes data, 4.3KB BSS (USB buffers)

---

## Current Firmware Behavior (Phase 3)

### On Boot
1. **2 quick blinks** - Boot indication (100ms ON/OFF)
2. **500ms pause**
3. **USB initialization** - Automatic via MX_USB_DEVICE_Init()
4. **3 quick heartbeat blinks** - Ready indication (50ms ON, 150ms OFF)

### In Main Loop - LED Write Protection Indicator
- **LED ON** - Write protection UNLOCKED (writes allowed)
- **LED OFF** - Write protection LOCKED (read-only mode)
- Updates every 100ms based on switch (PA2) state
- USB operations run automatically in background via interrupts

### USB Functionality
- Device enumerates as "STM32 FV1 Programmer v1.00"
- Exposes 8 blocks of 512 bytes (4KB total)
- Read/write operations map directly to EEPROM
- Write protection via hardware switch (PA2)

---

## Important Code Locations

### GPIO Debouncing (SysTick Integration)
File: `Core/Src/stm32l0xx_it.c:133`
```c
void SysTick_Handler(void)
{
  HAL_IncTick();
  GPIO_Update();  // Called every 1ms
}
```

### I2C Configuration
File: `Core/Src/main.c:185-216`
- I2C1: PB8 (SCL), PB9 (SDA) - Internal EEPROM
- I2C2: PB13 (SCL), PB14 (SDA) - FV1 EEPROM
- Timing: 0x00B07CB4 (configured by CubeMX)
- Mode: 7-bit addressing

### EEPROM Driver Configuration
File: `Core/Inc/eeprom_24lc32.h:28-33`
```c
#define EEPROM_I2C_HANDLE       hi2c1
#define EEPROM_I2C_ADDRESS      0x50
#define EEPROM_SIZE             4096
#define EEPROM_PAGE_SIZE        32
#define EEPROM_WRITE_TIMEOUT    10
```

### USB Storage Configuration
File: `USB_DEVICE/App/usbd_storage_if.c:71-73`
```c
#define FV1_STORAGE_BLK_NBR     8       // 4096 bytes / 512 bytes = 8 blocks
#define FV1_STORAGE_BLK_SIZ     512     // USB block size (standard)
```

Block Mapping:
```
USB Block 0 → EEPROM 0x0000-0x01FF
USB Block 1 → EEPROM 0x0200-0x03FF
USB Block 2 → EEPROM 0x0400-0x05FF
...
USB Block 7 → EEPROM 0x0E00-0x0FFF
```

---

### Phase 3: USB Mass Storage Device ✅

**Files Modified:**
- `USB_DEVICE/App/usbd_storage_if.c` - USB storage callbacks
- `Core/Src/main.c` - Updated for Phase 3 (removed Phase 2 test code)

**Features Implemented:**
```c
// Storage callbacks (all in usbd_storage_if.c)
int8_t STORAGE_Init_FS(uint8_t lun);              // Initialize EEPROM
int8_t STORAGE_GetCapacity_FS(...);               // Report 8 blocks × 512 bytes
int8_t STORAGE_IsReady_FS(uint8_t lun);           // Check EEPROM ready
int8_t STORAGE_IsWriteProtected_FS(uint8_t lun);  // Check hardware switch
int8_t STORAGE_Read_FS(...);                      // Map USB blocks to EEPROM
int8_t STORAGE_Write_FS(...);                     // Map USB blocks to EEPROM
```

**Implementation Approach:**
- **Option A (Raw Block Device)** - Direct block mapping
- USB Block 0 → EEPROM 0x0000-0x01FF (512 bytes)
- USB Block 1 → EEPROM 0x0200-0x03FF (512 bytes)
- ... through Block 7 → EEPROM 0x0E00-0x0FFF
- Write protection integrated via hardware switch (PA2)
- Device identifies as "STM32 FV1 Programmer v1.00"

**Testing Results:**
- ✅ Build successful (23.4KB flash, 312B data, 4KB BSS)
- ⏳ USB enumeration not yet tested
- ⏳ Read/write operations not yet tested
- ⏳ Write protection not yet tested

**Current Firmware Behavior:**
1. 2 quick blinks at boot (100ms ON/OFF)
2. 3 quick heartbeat blinks (50ms ON, 150ms OFF)
3. Main loop: slow heartbeat (50ms ON, 1950ms OFF)
4. USB stack runs automatically in background

**Future Enhancement:**
- Option B: Pre-formatted FAT12 filesystem for plug-and-play operation

---

## Remaining Work

### Phase 4: FV1 EEPROM Programming ⏳

**Objective:** Copy data from internal EEPROM to FV1 EEPROM via button press.

**Tasks:**
1. Create `fv1_programmer.h` and `fv1_programmer.c`
2. Implement I2C2 driver for FV1 EEPROM (similar to 24LC32AT)
3. Implement programming functions:
   - `FV1_EEPROM_Init()`
   - `FV1_EEPROM_WriteBuffer()`
   - `FV1_EEPROM_Verify()`
   - `FV1_ProgramFromInternal()`
4. Implement button-triggered programming sequence
5. Add LED feedback during programming
6. Block programming when switch is locked
7. Test with actual FV1 hardware

**Programming Sequence:**
- Short button press → Start programming
- LED indicates progress (alternating blink)
- Verification after write
- Success: LED1 solid
- Error: Very rapid blink

---

## Known Issues and Notes

### Critical Issues
1. **LED2 (PA1) Not Working** - Hardware issue, using only LED1
   - All status indication uses LED1 with different patterns
   - May need hardware rework or use different GPIO

### Important Notes
1. **I2C Pull-ups:** External 4.7kΩ pull-ups required on both I2C buses
2. **EEPROM Write Protection:** 24LC32AT WP pin must be connected appropriately
3. **USB Clock:** HSI48 used for USB (48MHz)
4. **CubeMX Compatibility:** TIM2 was disabled manually (PA0/PA1 initially configured as PWM)
5. **Build System:** Using Makefile (not CubeMX-generated IDE project)

### Development Environment
- Developed using Claude Code CLI
- Git repository initialized
- Clean working directory (no uncommitted changes)

---

## Testing Checklist

### Phase 1 Tests ✅
- [x] LED1 toggles with button press
- [x] LED1 reflects switch state
- [x] Both LEDs blink on startup (only LED1 works)
- [x] Button debouncing works reliably
- [x] Long press detection (2 seconds)

### Phase 2 Tests ✅
- [x] EEPROM detected on I2C
- [x] Write/read byte at address 0x0000
- [x] Write/read byte at address 0x0FFF
- [x] Write/read 64-byte buffer across page boundary
- [x] Page boundary handling verified
- [x] ACK polling working

### Phase 3 Tests ⏳ (Implementation Complete, Testing Pending)
- [x] Code implementation complete
- [x] Build successful (no errors)
- [ ] Device enumerates as USB Mass Storage
- [ ] Appears as removable drive on PC
- [ ] Can read 8 blocks (4KB) from device
- [ ] Can write to device when switch is unlocked
- [ ] Write-protected when switch is locked
- [ ] Data persists across power cycles
- [ ] EEPROM data correctly maps to USB blocks

### Phase 4 Tests ⏳ (Not Started)
- [ ] FV1 EEPROM detected on I2C2
- [ ] Button triggers programming sequence
- [ ] Data copied correctly to FV1 EEPROM
- [ ] Verification passes
- [ ] LED feedback shows correct status
- [ ] Programming blocked when locked

---

## Quick Start Guide (For Resuming Development)

### 1. Verify Build Environment
```bash
cd /home/saolin/STM32_EXP/Nymphes_fv1_programmer/Fv1_programmer
make clean && make -j4
# Should compile without errors
```

### 2. Current Firmware
- Location: `build/Fv1_programmer.bin`
- Status: Phase 3 USB Mass Storage Device (testing pending)
- What it does:
  - 2 quick blinks at boot
  - 3 quick heartbeat blinks (ready indication)
  - LED ON = write protection unlocked (writes allowed)
  - LED OFF = write protection locked (read-only)
  - USB Mass Storage device enumeration
  - Maps 8 USB blocks (512 bytes each) to internal EEPROM

### 3. Testing Phase 3
- Flash `build/Fv1_programmer.bin` to the device
- Connect USB cable (PA11/PA12)
- Check if device enumerates as "STM32 FV1 Programmer"
- Verify read/write operations
- Test write protection with switch (PA2)

### 4. To Continue to Phase 4
- Create `fv1_programmer.h` and `fv1_programmer.c`
- Implement I2C2 driver for FV1 EEPROM
- Add button-triggered programming sequence
- Test with actual FV1 hardware

### 4. Hardware Setup Required
- Ensure I2C1 has pull-up resistors (4.7kΩ) on PB8, PB9
- Ensure I2C2 has pull-up resistors (4.7kΩ) on PB13, PB14
- Connect FV1 EEPROM to I2C2
- Verify USB D+/D- connections

---

## Reference Information

### FV1 Program Structure
- FV1 supports up to 8 programs
- Each program is 512 bytes (128 instructions × 4 bytes)
- Total: 4096 bytes (perfectly matches our EEPROM)
- Programs stored sequentially in EEPROM

### Memory Map (Internal EEPROM)
```
0x0000 - 0x01FF : Program 0 (512 bytes)
0x0200 - 0x03FF : Program 1 (512 bytes)
0x0400 - 0x05FF : Program 2 (512 bytes)
0x0600 - 0x07FF : Program 3 (512 bytes)
0x0800 - 0x09FF : Program 4 (512 bytes)
0x0A00 - 0x0BFF : Program 5 (512 bytes)
0x0C00 - 0x0DFF : Program 6 (512 bytes)
0x0E00 - 0x0FFF : Program 7 (512 bytes)
```

### Useful HAL Functions
```c
// I2C
HAL_I2C_Master_Transmit()
HAL_I2C_Master_Receive()
HAL_I2C_IsDeviceReady()

// GPIO
HAL_GPIO_ReadPin()
HAL_GPIO_WritePin()
HAL_GPIO_TogglePin()

// Timing
HAL_Delay()
HAL_GetTick()
```

---

## Git Status
```
Current branch: main
Status: clean (no uncommitted changes)
Recent commits:
  4e2e9eb added gitignore + fix makefile
  215331a created project via CubeMX
  31cc18a Initial commit
```

---

## Contact / Notes

**Development Session:** 2025-12-29
**Developer:** Claude (Sonnet 4.5) + User (saolin)
**Next Session:** Continue with Phase 3 (USB Mass Storage Device)

**Important:** When resuming, review this document first to understand current state and any issues encountered.
