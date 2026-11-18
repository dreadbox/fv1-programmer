#!/usr/bin/env bash
# Production flash: erase all flash, program bootloader + app in one OpenOCD session,
# then write the FV-1 programs from ROM_DIR into the board's EEPROM over USB and verify.
# Requires an ST-Link probe, openocd and python3 (standard library only) in PATH.
# Does NOT build anything. Runs on macOS and Linux.
#
# Usage:
#   scripts/flash_production.sh [BOOTLOADER_ELF] [APP_ELF] [ROM_DIR]
#
# Defaults to Fv1_bootloader.elf, Fv1_programmer.elf and ROM/ in the same directory as this script.
#
# ROM_DIR holds up to 8 .bin/.fv1 programs (max 512 bytes each); other files are ignored.
# A numeric prefix followed by a separator picks the slot (0-7), e.g. "0_chorus.bin",
# "3-tremolo.bin". Files without a prefix fill the remaining slots in alphabetical order.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BOOTLOADER_ELF="${1:-${BOOTLOADER_ELF:-$SCRIPT_DIR/Fv1_bootloader.elf}}"
APP_ELF="${2:-${APP_ELF:-$SCRIPT_DIR/Fv1_programmer.elf}}"
ROM_DIR="${3:-${ROM_DIR:-$SCRIPT_DIR/ROM}}"

OPENOCD="${OPENOCD:-openocd}"
OPENOCD_INTERFACE="${OPENOCD_INTERFACE:-stlink}"
OPENOCD_TARGET="${OPENOCD_TARGET:-stm32l0}"
PYTHON="${PYTHON:-python3}"
USB_TIMEOUT="${USB_TIMEOUT:-15}"

# rom_tool check DIR          -- validate DIR and print the slot layout
# rom_tool write DIR TIMEOUT  -- wait for the board on USB, write the EEPROM image, read it back
rom_tool() {
    "$PYTHON" -I - "$@" <<'PY'
import glob, os, re, select, subprocess, sys, termios, time

VID, PID = 0x381E, 0x000A
SLOTS, SLOT_SIZE = 8, 512
EEPROM_SIZE = SLOTS * SLOT_SIZE
PROGRAM_EXTS = (".bin", ".fv1")
SLOT_PREFIX = re.compile(r"^(\d+)[ _.-]")


def fail(msg):
    sys.exit("ERROR: " + msg)


def build_image(rom_dir, show):
    if not os.path.isdir(rom_dir):
        fail("ROM folder not found: %s" % rom_dir)
    names = sorted((n for n in os.listdir(rom_dir)
                    if n.lower().endswith(PROGRAM_EXTS) and not n.startswith(".")),
                   key=str.casefold)
    if not names:
        fail("no .bin/.fv1 programs in %s" % rom_dir)
    if len(names) > SLOTS:
        fail("%d programs in %s, the EEPROM holds %d" % (len(names), rom_dir, SLOTS))

    slots = [None] * SLOTS
    unprefixed = []
    for name in names:
        m = SLOT_PREFIX.match(name)
        if not m:
            unprefixed.append(name)
            continue
        slot = int(m.group(1))
        if slot >= SLOTS:
            fail("%s: slot %d out of range (0-%d)" % (name, slot, SLOTS - 1))
        if slots[slot]:
            fail("%s and %s both claim slot %d" % (slots[slot], name, slot))
        slots[slot] = name
    free = [i for i, s in enumerate(slots) if s is None]
    for slot, name in zip(free, unprefixed):
        slots[slot] = name

    image = bytearray(EEPROM_SIZE)
    if show:
        print("ROM: %s" % rom_dir)
    for slot, name in enumerate(slots):
        if name is None:
            if show:
                print("  slot %d: (empty)" % slot)
            continue
        with open(os.path.join(rom_dir, name), "rb") as f:
            data = f.read()
        if not data or len(data) > SLOT_SIZE:
            fail("%s is %d bytes (must be 1-%d)" % (name, len(data), SLOT_SIZE))
        image[slot * SLOT_SIZE:slot * SLOT_SIZE + len(data)] = data
        if show:
            print("  slot %d: %s (%d B)" % (slot, name, len(data)))
    return bytes(image)


def find_ports():
    if sys.platform.startswith("linux"):
        ports = []
        for tty in glob.glob("/sys/class/tty/ttyACM*"):
            usb_dev = os.path.dirname(os.path.realpath(os.path.join(tty, "device")))
            try:
                with open(os.path.join(usb_dev, "idVendor")) as f:
                    vid = int(f.read(), 16)
                with open(os.path.join(usb_dev, "idProduct")) as f:
                    pid = int(f.read(), 16)
            except (OSError, ValueError):
                continue
            if (vid, pid) == (VID, PID):
                ports.append("/dev/" + os.path.basename(tty))
        return ports
    if sys.platform == "darwin":
        out = subprocess.run(["ioreg", "-r", "-c", "IOUSBHostDevice", "-l", "-w0"],
                             stdout=subprocess.PIPE, universal_newlines=True).stdout
        ports = []
        for block in re.split(r"^\+-o ", out, flags=re.M)[1:]:
            vid = re.search(r'"idVendor" = (\d+)', block)
            pid = re.search(r'"idProduct" = (\d+)', block)
            if vid and pid and (int(vid.group(1)), int(pid.group(1))) == (VID, PID):
                ports += re.findall(r'"IOCalloutDevice" = "([^"]+)"', block)
        return ports
    fail("unsupported platform: %s" % sys.platform)


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[1] = attrs[3] = 0  # raw: no input/output/line processing
        # HUPCL drops DTR on close, so LED1 ("host app connected") goes off when we exit.
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL | termios.HUPCL
        attrs[4] = attrs[5] = termios.B115200  # ignored by CDC
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)
    except Exception:
        os.close(fd)
        raise
    return fd


def write_all(fd, data, timeout=5.0):
    view = memoryview(data)
    deadline = time.monotonic() + timeout
    while view:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            fail("timed out writing to the device")
        if not select.select([], [fd], [], remaining)[1]:
            continue
        try:
            view = view[os.write(fd, view):]
        except BlockingIOError:
            pass


def read_bytes(fd, n, timeout, until=None):
    buf = bytearray()
    deadline = time.monotonic() + timeout
    while len(buf) < n:
        remaining = deadline - time.monotonic()
        if remaining <= 0 or not select.select([fd], [], [], remaining)[0]:
            break
        try:
            chunk = os.read(fd, n - len(buf))
        except BlockingIOError:
            continue
        if not chunk:
            break
        buf += chunk
        if until is not None and until in buf:
            break
    return bytes(buf)


def read_line(fd, timeout):
    return read_bytes(fd, 256, timeout, until=b"\n").decode("ascii", "replace").strip()


def connect(timeout):
    # After the OpenOCD reset the old USB device can linger for a moment, and on
    # Linux ModemManager may probe a new ttyACM port for a few seconds -- so wait,
    # then keep retrying the identify command until the board answers.
    time.sleep(2)
    deadline = time.monotonic() + timeout
    port = None
    while time.monotonic() < deadline:
        ports = find_ports()
        if len(ports) > 1:
            fail("more than one FV1 Programmer connected: %s" % ", ".join(ports))
        if ports:
            port = ports[0]
            try:
                fd = open_port(port)
            except PermissionError:
                fail("no permission to open %s (Linux: add your user to the 'dialout' group, "
                     "then log out and back in)" % port)
            except OSError:
                fd = None
            if fd is not None:
                termios.tcflush(fd, termios.TCIFLUSH)
                write_all(fd, b"I")
                if read_line(fd, 1.0).startswith("FV1_PROGRAMMER"):
                    return fd, port
                os.close(fd)
        time.sleep(0.5)
    if port:
        fail("%s did not answer the identify command" % port)
    fail("FV1 Programmer (USB %04X:%04X) did not show up within %d s" % (VID, PID, timeout))


def write_rom(fd, image):
    termios.tcflush(fd, termios.TCIFLUSH)
    # 'W' must travel in its own USB packet: the firmware only looks at the first
    # byte of the packet carrying a command and drops the rest of that packet.
    write_all(fd, b"W")
    time.sleep(0.05)
    for off in range(0, len(image), 64):
        write_all(fd, image[off:off + 64])
        time.sleep(0.005)
    resp = read_line(fd, 5.0)
    if resp != "OK":
        fail("EEPROM write failed (device answered %r)" % resp)


def verify_rom(fd, image):
    termios.tcflush(fd, termios.TCIFLUSH)
    write_all(fd, b"R")
    data = read_bytes(fd, len(image), 5.0)
    if len(data) != len(image):
        fail("read-back returned %d of %d bytes" % (len(data), len(image)))
    if data != image:
        bad = sorted({i // SLOT_SIZE for i in range(len(image)) if data[i] != image[i]})
        fail("read-back mismatch in slot(s) %s" % ", ".join(str(s) for s in bad))


def main():
    mode, rom_dir = sys.argv[1], sys.argv[2]
    image = build_image(rom_dir, show=(mode == "check"))
    if mode == "check":
        return
    fd, port = connect(float(sys.argv[3]))
    try:
        print("Writing ROM to %s ..." % port)
        write_rom(fd, image)
        verify_rom(fd, image)
    finally:
        os.close(fd)
    print("ROM written and verified.")


main()
PY
}

for f in "$BOOTLOADER_ELF" "$APP_ELF"; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: file not found: $f" >&2
        exit 1
    fi
done

for tool in "$OPENOCD" "$PYTHON"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: $tool not found in PATH" >&2
        exit 1
    fi
done

echo "Bootloader: $BOOTLOADER_ELF"
echo "App:        $APP_ELF"
rom_tool check "$ROM_DIR"
echo

"$OPENOCD" \
    -f "interface/${OPENOCD_INTERFACE}.cfg" \
    -f "target/${OPENOCD_TARGET}.cfg" \
    -c "init" \
    -c "reset halt" \
    -c "flash erase_address 0x08000000 0x10000" \
    -c "program \"$BOOTLOADER_ELF\" verify" \
    -c "program \"$APP_ELF\" verify reset" \
    -c "exit"

echo
echo "Waiting for the FV1 Programmer on USB (up to ${USB_TIMEOUT} s)..."
rom_tool write "$ROM_DIR" "$USB_TIMEOUT"

echo
echo "Done."
