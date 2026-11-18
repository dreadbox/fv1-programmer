# Nymphes FV-1 Programmer

USB programmer for the Spin Semiconductor FV-1 audio DSP. A custom STM32L053-based
board receives FV-1 audio programs over USB CDC into a local 24LC32 EEPROM, and on
a button press copies that EEPROM byte-for-byte into the EEPROM of an FV-1 target
mounted in the programmer socket.

A sibling **web app** ([`fv1-programmer-web-app`][webapp]) is the primary host
interface: load programs into the local EEPROM, send them to the FV-1 target,
and update the programmer's firmware -- all from a Chromium-based browser via
Web Serial. The on-device firmware exposes a USB-CDC command protocol (see
[PROTOCOL.md](PROTOCOL.md)); a couple of CLI scripts under `scripts/` cover
the same workflow for headless / debug use.

[webapp]: https://github.com/giuanz/fv1-programmer-web-app

## Hardware

| Component | Connection |
|---|---|
| MCU | STM32L053C8 (Cortex-M0+, 64 KB flash, 8 KB RAM) |
| Local EEPROM (source) | 24LC32 on I2C1 |
| FV-1 target EEPROM | EEPROM in the FV-1 socket, on I2C2 |
| LED1 | PA0 -- host port-open indicator |
| LED2 | PA1 -- activity / error indicator |
| Button | PB11, active LOW (pull-up) -- triggers copy / forces bootloader at power-on |
| USB | Full-speed device, internal HSI48 |
| Debug | SWD via ST-Link |

## Architecture

Two independent firmware images sharing the 64 KB flash:

| Region | Address | Size | Purpose |
|---|---|---|---|
| Bootloader | `0x08000000` | 16 KB | USB-CDC firmware updater, jump-to-app |
| App metadata | `0x08003F80` | 128 B | `{magic, size, crc32, version}` written by host |
| Application | `0x08004000` | 48 KB | Normal operation: USB protocol, EEPROM copy |

The bootloader runs on every reset. It jumps to the app unless any of:
boot-request magic word is in RAM (set by app's `U` command before
`NVIC_SystemReset`), PB11 is held LOW, or the app's reset vector is out of
flash bounds. The Cortex-M0+ has no VTOR, so the app relocates its vector
table to SRAM via SYSCFG memory remap at `main()` entry.

Each firmware enumerates as a CDC virtual serial port with a different USB PID,
so the host can tell which one is running:

| PID | Mode |
|---|---|
| `0x000A` | Application |
| `0x800A` | Bootloader |

## Building

Requires `arm-none-eabi-gcc`, `make`, `openocd`, and an ST-Link probe.

```sh
cd Fv1_bootloader && make    # builds Fv1_bootloader/build/Fv1_bootloader.{elf,hex,bin}
cd Fv1_programmer && make    # builds Fv1_programmer/build/Fv1_programmer.{elf,hex,bin}
```

Each subdirectory has its own Makefile, linker script, and HAL configuration --
the two builds are fully independent.

## Flashing

### First-time install (ST-Link, full chip wipe)

```sh
cd Fv1_bootloader && make flash_erase   # erase all flash then program bootloader
cd Fv1_programmer && make flash         # program app at 0x08004000
```

The bootloader sits in flash permanently after this. Subsequent updates of
either firmware don't touch the other region.

### Iterative app development (ST-Link, fast)

```sh
cd Fv1_programmer && make flash
```

`make flash` only writes the regions in the app ELF (`0x08004000+`), leaving
the bootloader untouched. After flashing, the bootloader runs, sees a valid
reset vector, and jumps straight to the new app. Roughly 3-5 s per cycle.

### USB OTA update (no ST-Link needed)

Two paths, both implement the same sequence (identify -> erase -> program ->
metadata -> CRC -> jump):

**Via the web app** -- pick the `.bin`, click Update Firmware. The app reboots
the device into its bootloader, flashes, and reconnects automatically. The
build number is read from a stamp embedded in the `.bin` (see "App version
stamp" in [PROTOCOL.md](PROTOCOL.md)) so the user never picks a version.

**Via the CLI scripts:**

```sh
python3 scripts/reboot_to_bootloader.py /dev/cu.usbmodem<APP_PORT>
python3 scripts/firmware_update.py /dev/cu.usbmodem<BOOTLOADER_PORT> \
    Fv1_programmer/build/Fv1_programmer.bin --version 1
```

Either way: if the app firmware is unresponsive or missing, hold PB11 while
power-cycling to force the bootloader directly.

To bump the build number that ships in a `.bin`, edit the `version` field of
`__app_version_stamp` in `Fv1_programmer/Core/Src/main.c` and rebuild.

## Using the device

### LED meanings

**LED1 (PA0)** -- USB host application status:
- OFF: no cable, cable to a charger, or no app has the COM port open
- ON: a host application has opened the virtual COM port (DTR asserted)

**LED2 (PA1)** -- operation status:
- OFF: idle
- ~5 Hz blink: EEPROM write (USB) or button-triggered copy in progress
- Solid ON for 3 s: last operation failed (cleared early if a new operation starts)

### Button

- **Short press**: copy the contents of the local 24LC32 (I2C1) into the FV-1
  target's EEPROM (I2C2). Requires a working FV-1 target on I2C2; if none is
  present the copy fails fast (~50 ms) and LED2 lights solid for 3 s.
- **Held at power-on**: forces bootloader mode regardless of app validity.

### USB CDC commands (app mode)

| Cmd | Effect |
|---|---|
| `R` | Read full 4 KB EEPROM, streamed back over CDC |
| `W` + 4096 bytes | Write 4 KB to the local EEPROM |
| `C` | Copy local EEPROM to the FV-1 target (same as pressing the button). Blocks up to ~2 s; responds `COPY_OK\n` or `COPY_FAIL\n` |
| `?` | Return running firmware version from app metadata as `VERSION <N>\n`, or `META_INVALID\n` if the metadata block at `0x08003F80` has no valid magic |
| `U` | Acknowledge with `REBOOTING...\n`, then reboot into bootloader (for OTA firmware update) |

The bootloader speaks a separate command set (`B`/`V`/`E`/`P`/`M`/`C`/`G`)
used by the firmware updater. See [PROTOCOL.md](PROTOCOL.md) for the full
byte-level specification of both protocols, the update sequence, and the
boot-request mechanism.
