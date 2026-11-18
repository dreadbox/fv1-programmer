# USB CDC Protocol -- Protocol v1

Byte-level specification of the two USB CDC protocols spoken by this firmware:
one when the **application** is running (PID `0x000A`) and one when the
**bootloader** is running (PID `0x800A`). The two are mutually exclusive --
only one is enumerated at a time. Both use the same USB VID (`0x381E`,
Dreadbox) and the same baseline CDC class so any host driver that
speaks "generic USB serial" works without per-device drivers.

Reference implementations: [`scripts/firmware_update.py`](scripts/firmware_update.py)
and [`scripts/reboot_to_bootloader.py`](scripts/reboot_to_bootloader.py).

## Transport

| Property | Value |
|---|---|
| Device class | USB CDC ACM, Full Speed |
| VID | `0x381E` |
| PID (app) | `0x000A` |
| PID (bootloader) | `0x800A` |
| Endpoint MPS | 64 bytes (bulk IN and OUT) |
| Line coding | Irrelevant (CDC is packet-based here); pyserial defaults work |
| DTR | Asserted when host application opens the port. The app lights LED1 on DTR; bootloader ignores it. |

**Multi-packet receive.** A single command longer than 64 bytes (notably `P`,
133 B; `M`, 129 B) arrives as multiple USB OUT packets. The firmware
accumulates bytes per command until it has the expected total length, then
dispatches. Hosts may write the whole command in one `write()` call --
pyserial handles fragmentation transparently. No inter-packet delay is
required between fragments of the same command.

**Response framing.** Every command returns a single ASCII line terminated
by `\n` (no `\r`). Responses fit in one IN packet (≤ 64 bytes). Hosts
should read until `\n`.

**Timing.** All command handlers run synchronously in the main loop after
the receive callback queues them. Worst-case latency:

| Command | Typical | Worst case |
|---|---|---|
| App `T`/`I`/`?` | < 1 ms | < 5 ms |
| App `R` (4 KB) | ~ 50 ms | ~ 200 ms (I2C-bound) |
| App `W` (4 KB) | ~ 700 ms | ~ 1.5 s (32×128 B page writes × 5 ms each) |
| App `C` (4 KB I2C1 → I2C2) | ~ 700 ms | ~ 2 s; fails fast in ~ 50 ms if no FV-1 target |
| App `U` | < 50 ms before USB drops | -- |
| Bootloader `B`/`V` | < 1 ms | < 5 ms |
| Bootloader `E` (1 page) | ~ 3 ms | ~ 10 ms |
| Bootloader `P` (1 page) | ~ 3 ms | ~ 10 ms |
| Bootloader `M` (erase + 128 B) | ~ 8 ms | ~ 20 ms |
| Bootloader `C` (4 KB checksum) | ~ 5 ms | ~ 15 ms |
| Bootloader `G` | 50 ms before USB drops | -- |

USB re-enumeration after `U` (app → bootloader) or `G` (bootloader → app)
takes **~ 500–1500 ms** depending on host. Allow at least 2 s before
attempting to open the new port.

## Application protocol (PID `0x000A`)

| Cmd | Payload | Response on success | Response on error |
|---|---|---|---|
| `T` | -- | `TEST_OK\n` | -- |
| `I` | -- | `FV1_PROGRAMMER_V1.0\n` | -- |
| `?` | -- | `VERSION <N>\n` where `<N>` is decimal `uint32` from `AppMetadata.version` | `META_INVALID\n` if metadata magic ≠ `0xDEADBEEF` |
| `R` | -- | 4096 raw bytes streamed in up to 1024 B chunks, no framing | (no response on EEPROM read failure -- stream just stops) |
| `W` + 4096 B | 4096 raw bytes | `OK\n` after the last byte is committed | `ERROR\n` if any chunk write fails or more than 4096 B is received |
| `C` | -- | `COPY_OK\n` after the local EEPROM is copied to the FV-1 target over I2C2 | `COPY_FAIL\n` (no target attached, I2C error, or per-page timeout) |
| `U` | -- | `REBOOTING...\n` followed ~50 ms later by USB disconnect; bootloader appears within ~ 2 s | -- |

Notes:
- `R` and `W` operate on the local 24LC32 EEPROM (I2C1, 4096 B total).
- `W` data may be split across multiple `write()` calls on the host side;
  internally it is accumulated in 1024 B chunks and flushed to the EEPROM
  after each chunk. Status only flips to `OK\n` once all 4096 bytes are in.
- The `W` byte must arrive in its own USB packet: the firmware looks only
  at the first byte of the packet carrying a command and drops the rest of
  that packet. Send `W`, pause briefly (~50 ms), then the 4096 data bytes.
- `?` lets the host compare the running firmware against an image it
  intends to upload, so it can skip no-op updates.
- `U` is acknowledged *before* `NVIC_SystemReset` runs so the host can
  reliably know the request was accepted. After the ACK arrives, close
  the port -- it will disappear shortly.
- Unknown bytes are silently dropped (no `UNKNOWN` response in app mode).

## Bootloader protocol (PID `0x800A`)

All multi-byte fields are **little-endian**.

| Cmd | Payload | Response on success | Response on error |
|---|---|---|---|
| `B` | -- | `BOOTLOADER_V1.0\n` | -- |
| `V` | -- | `APP_VALID\n` | `APP_INVALID\n` (any failure: bad magic, size out of range, CRC mismatch, bad reset vector) |
| `E` + 4 B addr (LE) | -- | `ERASE_OK\n` | `ERASE_FAIL\n` (addr below `0x08003F80`, addr unaligned, or HAL erase failure) |
| `P` + 4 B addr (LE) + 128 B data | -- | `PROG_OK\n` | `PROG_FAIL\n` (addr below `0x08003F80`, addr unaligned to 128 B, or any word-program failure) |
| `M` + 128 B metadata | -- | `META_OK\n` | `META_FAIL\n` (erase or program failure on the metadata page) |
| `C` + 4 B expected CRC (LE) | -- | `CRC_OK\n` (metadata magic valid AND CRC over `app_size` bytes starting at `0x08004000` matches both `AppMetadata.crc32` and the supplied expected CRC) | `CRC_MISMATCH\n` |
| `G` | -- | `JUMPING...\n` followed ~50 ms later by USB disconnect and a jump to the app reset vector | (silent abort if app reset vector is out of flash bounds) |
| any other byte | -- | `UNKNOWN\n` | -- |

Notes:
- `E` and `P` both reject any address below `BOOT_META_ADDR` (`0x08003F80`),
  so the bootloader cannot be erased or overwritten through this protocol.
- `P` requires the address to be 128 B aligned (page boundary).
- `M` erases the single 128 B metadata page before writing -- the caller
  does not need to issue a separate `E` for it.
- `C` reads `app_size` from the metadata block. The `M` command must be
  issued before `C` can succeed.
- `G` does *not* validate metadata. The boot-time decision uses only the
  reset-vector range check (see `should_stay_in_bootloader()`); the metadata
  CRC is purely advisory via `V` and `C`.

## App metadata block

Written by the host at `0x08003F80` via the `M` command. 128 bytes total.

```c
typedef struct {
    uint32_t magic;        // 0xDEADBEEF (little-endian on the wire)
    uint32_t app_size;     // Padded size in bytes, multiple of 128
    uint32_t crc32;        // CRC32 over the padded app image (see "CRC" below)
    uint32_t version;      // Monotonic uint32, host-defined semantics
    uint8_t  reserved[112]; // Must be 0x00 -- reserved for protocol evolution
} AppMetadata_t;
```

The host computes `crc32` over the same byte range and value that ends up
in flash: the application binary padded with `0xFF` to a 128 B boundary,
of length `app_size`. The bootloader recomputes the CRC across
`app_size` bytes starting at `0x08004000` for `C` verification.

## App version stamp

To let the host show a meaningful build identifier without requiring the
user to type a version number at update time, the application firmware
embeds a small struct at a fixed offset inside its own `.bin`:

| Field | Type | Notes |
|---|---|---|
| `magic` | `uint32` LE | `0x53524556` -- ASCII bytes `V`, `E`, `R`, `S` in file order |
| `version` | `uint32` LE | Monotonic build number, set by the firmware author |

The struct is placed in a `.app_version` linker section that comes
immediately after `.isr_vector`, so the stamp lives at file offset
`0xC0` (= vector table length of 48 × 4 bytes). The host MAY scan the
first 512 bytes of the `.bin` for the magic word rather than relying on
a fixed offset, so the convention survives minor build-time
rearrangements.

Hosts that find the stamp SHOULD use the embedded `version` as the
value written into the `AppMetadata.version` field via the bootloader's
`M` command. Hosts that don't find the stamp (older firmware images,
or `.bin` files produced before this convention) MAY fall back to any
other version source (auto-increment from the currently installed
metadata, etc.).

## Boot-request mechanism

To force the bootloader from the app side without holding PB11, the app
writes a magic word to a fixed RAM address and triggers a software reset.
This RAM word is not zeroed by startup code, so it survives across the
reset.

| | Value |
|---|---|
| Magic | `0xB00710AD` |
| Address | `0x20001F00` |
| Bootloader behavior | Reads the word, clears it, treats the magic as "stay in bootloader" |

The app's `U` command implements this sequence (see
`Fv1_programmer/Core/Src/main.c:36-40`). Host code never touches this --
just send `U` over CDC.

## CRC

CRC-32 (IEEE 802.3, polynomial `0xEDB88320`, init `0xFFFFFFFF`, reflected
in and out, final XOR `0xFFFFFFFF`). This matches Python's
`zlib.crc32`. The STM32L0 hardware CRC peripheral is configured to
produce the same value via `CRC_INPUTDATA_INVERSION_BYTE` +
`CRC_OUTPUTDATA_INVERSION_ENABLE` + a final `^ 0xFFFFFFFF` (see
`Fv1_bootloader/Core/Src/crc32.c`).

## Full firmware update sequence

The canonical update flow as implemented in
`scripts/firmware_update.py`. A second implementation MUST follow the
same ordering -- the bootloader does not gate the order, but a host
that diverges from it will end up in an inconsistent state on partial
failure.

```
host                                    device (app, PID 0x000A)
  |                                      |
  | ----------- ?  --------------------> |    (optional pre-check)
  | <-------- VERSION 7\n -------------- |
  | ----------- U  --------------------> |
  | <------- REBOOTING...\n ------------ |
  |                                      X  (USB disconnect, ~50 ms)
  |
  |   ... wait ~ 1-2 s for bootloader enumeration on PID 0x800A ...
  |
  |                                      | (bootloader, PID 0x800A)
  | ----------- B  --------------------> |
  | <----- BOOTLOADER_V1.0\n ----------- |
  |
  |   for each 128 B page in padded image:
  | ----- E + page_addr (LE) ----------> |
  | <-------- ERASE_OK\n --------------- |
  |
  |   for each 128 B page in padded image:
  | --- P + page_addr (LE) + 128 B ----> |
  | <-------- PROG_OK\n ---------------- |
  |
  | ----- M + 128 B metadata ----------> |
  | <-------- META_OK\n ---------------- |
  | ----- C + expected_crc (LE) -------> |
  | <-------- CRC_OK\n ----------------- |
  | ----------- G  --------------------> |
  | <------- JUMPING...\n -------------- |
  |                                      X  (USB disconnect, ~50 ms)
  |
  |   ... wait ~ 1-2 s for app enumeration on PID 0x000A ...
```

**Padding.** The on-device flash page size is 128 B. Hosts must pad the
firmware binary up to a 128 B boundary with `0xFF` before computing
CRC32 and length. Both `app_size` in metadata and the per-page address
math must reflect the padded length.

**Erase-then-program.** The bootloader does **not** auto-erase before
`P`; the host MUST issue an `E` for every page it intends to write. The
reference implementation erases all pages first, then programs all
pages, to keep the two phases visually distinct in the UI.

**Failure recovery.** Any error response from `E`, `P`, `M`, or `C`
leaves the device in bootloader mode with whatever partial state was
written. The host can retry from the beginning (the `E` calls will
re-erase). The metadata block is the last thing written before `C`, so
a power loss before `M` leaves the previous valid metadata in place and
the device falls back to whatever was running before -- but the
application image at `0x08004000` will be corrupt, so the bootloader's
`V` command will report `APP_INVALID` and the boot-time reset-vector
check will fail, keeping the device in bootloader until a new upload
completes.

## Protocol versioning

This document specifies **Protocol v1**. The `B` and `I` response
strings encode `V1.0`. Future incompatible changes must:

1. Bump the version string returned by `B` / `I`.
2. Add a new top-level command rather than redefining an existing one --
   the bootloader's `default` case returns `UNKNOWN\n`, which hosts can
   use as a capability probe.
3. Document the new command here with its minimum required protocol
   version.

Reserved-for-future-use bytes in commands or metadata MUST be sent as
`0x00`. Reserved-for-future-use cases in responses MUST NOT be relied
on by hosts.
