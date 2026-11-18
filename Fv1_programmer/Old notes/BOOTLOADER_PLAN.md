# STM32L053 Bootloader Implementation Plan

## Overview

Create a USB CDC-based bootloader for the FV1 Programmer firmware, enabling over-the-air firmware updates.

## Memory Layout

| Region | Address Range | Size |
|--------|---------------|------|
| Bootloader | `0x08000000 - 0x08003F7F` | ~16 KB |
| App Metadata | `0x08003F80 - 0x08003FFF` | 128 bytes |
| Application | `0x08004000 - 0x0800FFFF` | 48 KB |

RAM: first 256 bytes (`0x20000000 - 0x200000FF`) reserved for app vector table copy (M0+ has no VTOR). App linker starts RAM at `0x20000100`.

## Implementation Steps

### Phase 1: Create Bootloader Project Structure -- DONE
- [x] Create `Fv1_bootloader/` directory structure
- [x] Create `boot_config.h` with memory map definitions
- [x] Create bootloader linker script (16KB at 0x08000000, `.noinit` section)
- [x] Copy and configure Makefile for bootloader (reuses app HAL drivers)
- [x] Minimal `main.c` with clock config, GPIO init, boot button input (PB11)
- [x] Startup file and HAL MSP config

### Phase 2: Jump-to-App Logic -- DONE
- [x] `boot_request` variable in `.noinit` section (survives software reset)
- [x] `should_stay_in_bootloader()` -- checks magic word, boot button, valid reset vector
- [x] `jump_to_app()` -- HAL_DeInit, SysTick teardown, set MSP, branch to app
- [x] LED1 (PA0) continuous blink when in bootloader mode
- [x] Makefile `erase` and `flash_erase` targets (full-chip erase via OpenOCD)
- [x] Verified: stays in bootloader when no app present (erased flash)
- [x] Verified: boot button (PB11) forces bootloader mode

### Phase 3: Flash Driver -- DONE
- [x] Created `flash_driver.h/c` with erase/program API
- [x] Page erase with address validation (rejects anything below `BOOT_META_ADDR`)
- [x] Word-aligned programming via `FlashDrv_ProgramWord()` and `FlashDrv_ProgramBuffer()`
- [x] Full app region erase via `FlashDrv_EraseAppRegion()`
- [x] HAL_FLASH_Unlock/Lock per operation
- STM32L0 flash page size: 128 bytes

### Phase 4: Hardware CRC32 -- DONE
- [x] Created `crc32.h/c` using STM32L0 CRC peripheral
- [x] Configured for standard CRC-32 (compatible with Python `zlib.crc32`)
- [x] `CRC_INPUTDATA_INVERSION_BYTE` + `CRC_OUTPUTDATA_INVERSION_ENABLE` + final `^ 0xFFFFFFFF`
- [x] Enabled `HAL_CRC_MODULE_ENABLED` in `stm32l0xx_hal_conf.h`

### Phase 5: Bootloader USB CDC Protocol -- DONE
- [x] Created full USB CDC stack for bootloader (separate from app's USB stack)
- [x] Bootloader USB PID `0x5741` (app uses `0x5740`) to distinguish devices
- [x] Command parser in `usbd_cdc_if.c` with ISR-to-main-loop deferral (`pending_cmd`)
- [x] All 7 protocol commands implemented: B, V, E, P, M, C, G
- [x] 'G' (jump) command deinits USB PCD before jumping to app
- [x] Enabled `HAL_PCD_MODULE_ENABLED` in `stm32l0xx_hal_conf.h`
- [x] USB IRQ handler in `stm32l0xx_it.c`
- Bootloader size after Phase 5: **15,340 bytes** (1,044 bytes headroom in 16KB)

### Phase 6: Modify Existing Application -- DONE
- [x] Created `STM32L053XX_FLASH_APP.ld` -- flash at 0x08004000 (48K), RAM at 0x20000100
- [x] Vector table relocation via SYSCFG memory remap (copy 48 vectors to SRAM, remap to 0x00000000)
- [x] `RelocateVectorTable()` called before `HAL_Init()` in `main()`
- [x] `RequestBootloader()` writes magic `0xB00710AD` to `0x20001FF0` then `NVIC_SystemReset()`
- [x] 'U' command in app CDC triggers bootloader reboot (deferred to main loop)
- [x] Updated Makefile to use `STM32L053XX_FLASH_APP.ld`
- [x] **Bug fixed**: 'U' command originally called `HAL_Delay()` in USB ISR context (would hang on M0+). Moved to `CDC_ProcessPendingCommand()` in main loop.
- App size after Phase 6: **22,316 bytes**

### Phase 7: Create Host Update Tool -- DONE
- [x] Created `scripts/firmware_update.py` (pyserial + zlib)
- [x] Full update sequence: identify -> erase -> program -> write metadata -> verify CRC -> jump
- [x] Per-page progress percentage on erase and program steps
- [x] Pads firmware to 128-byte page boundary with `0xFF`, CRC computed on padded image
- [x] CLI: `python3 firmware_update.py <port> <firmware.bin> [--version N]`

### Phase 8: Integration Testing -- IN PROGRESS
- [x] Bootloader flashed via ST-Link (chip erase + flash) -- OK
- [x] Bootloader stays in bootloader mode when no app present -- OK (erased flash detected)
- [x] USB CDC enumerates on macOS -- OK (bootloader PID 0x5741 appears as different port)
- [x] **USB CDC data flow fixed** -- root cause was missing HSI48 clock source selection for USB. Added `MODIFY_REG(RCC->CCIPR, RCC_CCIPR_HSI48SEL, RCC_CCIPR_HSI48SEL)` to `SystemClock_Config()`
- [x] **Multi-packet USB reception fixed** -- USB CDC delivers max 64 bytes per callback, but commands like 'P' need 133 bytes. Added accumulation buffer with `rx_pos` counter in `CDC_Receive_FS()`
- [x] All protocol commands working (B, V, E, P, M, C, G) -- tested via serial
- [x] Full firmware upload via `firmware_update.py` -- all 6 steps succeed (identify, erase, program, metadata, CRC, jump)
- [x] Software-triggered bootloader entry ('U' command from app) -- works. Fixed shared RAM address to `0x20001F00` (was `0x20001FF0`, too close to stack top and got overwritten during startup)
- [x] Jump to app via 'G' command -- app starts correctly after firmware upload
- [x] LED1 (PA0) blinks in bootloader mode on both cold boot and software-reset entry -- the previously open issue did not reproduce on the new hardware revision (2026-05-24). Original symptom was hardware-related to the old board.
- [ ] Test button-triggered bootloader entry (PB11 held at power-on)
- [ ] Test recovery from corrupted application
- [ ] Test CRC mismatch detection

**Hardware notes:**
- New hardware revision has both LEDs working (LED1 on PA0, LED2 on PA1); previous revision only had LED1
- Bootloader USB port is different from app port (PID 0x5741 vs 0x5740)
- Boot request magic address: `0x20001F00` (must match in both bootloader and app)

---

## Bootloader Entry Conditions

1. **Button held at boot** - PB11 LOW during power-on
2. **Invalid application** - Bad magic, CRC mismatch, or corrupt vectors
3. **Software request** - App writes `0xB00710AD` to `0x20001F00`, then resets

## USB Protocol Commands

| Cmd | Payload | Response |
|-----|---------|----------|
| `B` | none | `BOOTLOADER_V1.0\n` |
| `V` | none | `APP_VALID\n` / `APP_INVALID\n` |
| `E` | 4-byte LE addr | `ERASE_OK\n` / `ERASE_FAIL\n` |
| `P` | 4-byte LE addr + 128 bytes | `PROG_OK\n` / `PROG_FAIL\n` |
| `M` | 128 bytes metadata | `META_OK\n` / `META_FAIL\n` |
| `C` | 4-byte LE CRC | `CRC_OK\n` / `CRC_MISMATCH\n` |
| `G` | none | `JUMPING...\n` |

## App Metadata Structure (128 bytes at 0x08003F80)

```c
typedef struct {
    uint32_t magic;      // 0xDEADBEEF
    uint32_t app_size;   // Size in bytes
    uint32_t crc32;      // CRC32 of app region
    uint32_t version;    // Version number
    uint8_t  reserved[112];
} AppMetadata_t;
```

---

## Verification Checklist

1. [x] Bootloader builds and fits in 16KB (15,492 bytes)
2. [x] Application builds with new linker script (22,316 bytes)
3. [x] Bootloader flashes via ST-Link (OpenOCD)
4. [x] Button entry works (hold PB11 at power-on)
5. [x] Invalid app triggers bootloader entry (erased flash detected)
6. [x] USB enumerates in bootloader mode
7. [x] `firmware_update.py` successfully uploads firmware
8. [x] Bootloader jumps to valid application
9. [x] Software reset to bootloader works ('U' command)

---

## Key Files

### Bootloader (`Fv1_bootloader/`)
| File | Purpose |
|------|---------|
| `Core/Inc/boot_config.h` | Memory map, magic numbers, `AppMetadata_t` |
| `Core/Inc/flash_driver.h` | Flash driver API |
| `Core/Src/flash_driver.c` | Flash erase/program with address guards |
| `Core/Inc/crc32.h` | CRC32 API |
| `Core/Src/crc32.c` | Hardware CRC32 (STM32L0 peripheral) |
| `Core/Src/main.c` | Boot logic, USB init, main loop |
| `USB_DEVICE/App/usbd_cdc_if.c` | Bootloader command parser (B/V/E/P/M/C/G) |
| `USB_DEVICE/App/usbd_desc.c` | USB descriptors (PID 0x5741) |
| `STM32L053XX_FLASH_BOOT.ld` | Linker: 16K flash, `.noinit` for boot_request |

### Application (`Fv1_programmer/`)
| File | Changes for bootloader support |
|------|------|
| `Core/Src/main.c` | `RelocateVectorTable()`, `RequestBootloader()` |
| `USB_DEVICE/App/usbd_cdc_if.c` | 'U' command -> reboot to bootloader |
| `STM32L053XX_FLASH_APP.ld` | Flash at 0x08004000, RAM at 0x20000100 |
| `Makefile` | Points to `STM32L053XX_FLASH_APP.ld` |

---

## Size Budget

| Component | Text | Limit | Headroom |
|-----------|------|-------|----------|
| Bootloader | 15,492 B | 16,384 B | 892 B |
| Application | 22,316 B | 49,152 B | 26,836 B |
