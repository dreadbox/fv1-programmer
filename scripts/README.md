# Scripts

Helpers for programming and updating the FV1 Programmer firmware.

## `flash_production.sh` -- factory flash of blank hardware

One command takes a blank board to a finished unit:

1. Erases all 64 KB of flash and programs the bootloader and the application
   in a single OpenOCD session.
2. Waits for the application to enumerate over USB, writes the FV-1 programs
   from the ROM folder into the board's 24LC32 EEPROM, and reads them back to
   verify.

The ROM folder is checked before the chip is touched, so a bad folder never
leaves a half-flashed board. Runs on macOS and Linux (tested on macOS 13).

### Requirements

- An ST-Link probe connected to the board's SWD header, and the board's USB
  port connected to the same computer
- `openocd` and `python3` on `PATH` (standard library only -- no `pip install`)
- Next to this script:
  - `Fv1_bootloader.elf`
  - `Fv1_programmer.elf`
  - `ROM/` -- the FV-1 programs to load

### ROM folder

- Up to 8 programs, `.bin` or `.fv1`, 1-512 bytes each. Other files
  (`.spn` sources, hidden files) are ignored.
- A number at the start of the file name, followed by `_`, `-`, `.` or a space,
  picks the slot (0-7): `0_chorus.bin`, `3-tremolo.bin`.
- Files without a prefix fill the remaining slots in alphabetical order
  (case-insensitive).
- Unused slots are filled with `0x00`.
- Two files claiming the same slot, a slot number above 7, or more than 8
  programs stop the script with an error.

The slot layout is printed before flashing starts.

### Usage

From this directory:

```sh
./flash_production.sh
```

To use specific files or another ROM folder instead of the defaults:

```sh
./flash_production.sh /path/to/bootloader.elf /path/to/app.elf /path/to/rom_folder
```

Equivalent via environment variables:

```sh
BOOTLOADER_ELF=/path/to/bootloader.elf \
APP_ELF=/path/to/app.elf \
ROM_DIR=/path/to/rom_folder \
./flash_production.sh
```

Other overridable variables (rarely needed):

- `OPENOCD` -- path to the `openocd` binary (default: `openocd` from `PATH`)
- `OPENOCD_INTERFACE` -- interface config name (default: `stlink`)
- `OPENOCD_TARGET` -- target config name (default: `stm32l0`)
- `PYTHON` -- Python 3 interpreter (default: `python3` from `PATH`)
- `USB_TIMEOUT` -- seconds to wait for the board to answer on USB after
  flashing (default: `15`)

The board is found by its USB IDs (`0x381E:0x000A`), so the ST-Link's own
serial port is never touched. Only one FV1 Programmer may be connected at a
time.

### Linux setup (once per machine)

- Serial port access: add the operator account to the `dialout` group, then
  log out and back in:

  ```sh
  sudo usermod -aG dialout "$USER"
  ```

- ModemManager (installed by default on Ubuntu) probes every new
  `/dev/ttyACM*` port with modem commands for a few seconds, which can collide
  with the EEPROM write. Tell it to ignore this board:

  ```sh
  sudo tee /etc/udev/rules.d/99-fv1-programmer.rules >/dev/null <<'RULES'
  ATTRS{idVendor}=="381e", ATTRS{idProduct}=="000a", ENV{ID_MM_DEVICE_IGNORE}="1"
  ATTRS{idVendor}=="381e", ATTRS{idProduct}=="800a", ENV{ID_MM_DEVICE_IGNORE}="1"
  RULES
  sudo udevadm control --reload-rules && sudo udevadm trigger
  ```

- ST-Link access without `sudo`: the distribution's `openocd` package installs
  the udev rules for ST-Link probes; replug the probe after installing it.

## USB OTA update (no ST-Link)

For in-field updates of an already-flashed device, use the two Python
helpers in this folder:

- `reboot_to_bootloader.py` -- sends `U` to the running app to drop it into
  the bootloader.
- `firmware_update.py` -- uploads a `.bin` over USB CDC.

See the "USB OTA update" section of the top-level `README.md` for the full
workflow and port-naming conventions.
