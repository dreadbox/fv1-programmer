# Firmware Update: Write Cache Disabled via SCSI MODE SENSE

## Problem Identified

The data persistence issue was caused by **OS-level write caching**:

1. **Without O_SYNC flag:**
   - OS caches file writes in RAM
   - Returns "success" immediately to application
   - Schedules actual write to USB device for later
   - If device is disconnected/powered-off before flush, writes are lost

2. **With O_SYNC flag (what worked in your test):**
   - OS performs synchronous writes
   - Waits for device to confirm completion before returning
   - Data guaranteed written before returning to application
   - **Data persists correctly!**

The mystery "incrementing pattern" (0x20, 0x21, 0x22...) was **OLD DATA** already in EEPROM that was never actually overwritten due to the caching issue.

## Solution Implemented

Modified the **SCSI MODE SENSE** response to include a **Caching Mode Page (0x08)** with the **WCE (Write Cache Enable) bit set to 0**.

This tells the operating system: *"This device has NO write cache - treat ALL writes as synchronous"*

### Files Modified

1. **Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc_data.c**
   - Updated `MSC_Mode_Sense6_data[]` array (8 → 12 bytes)
   - Updated `MSC_Mode_Sense10_data[]` array (8 → 16 bytes)
   - Added Caching Mode Page with WCE=0

2. **Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Inc/usbd_msc_data.h**
   - Updated `MODE_SENSE6_LEN` from 8U to 12U
   - Updated `MODE_SENSE10_LEN` from 8U to 16U

3. **Middlewares/ST/STM32_USB_Device_Library/Class/MSC/Src/usbd_msc_scsi.c**
   - Updated `SCSI_ModeSense6()` to use `MODE_SENSE6_LEN` instead of hardcoded 8U
   - Updated `SCSI_ModeSense10()` to use `MODE_SENSE10_LEN` instead of hardcoded 8U

### Technical Details

**MODE SENSE 6 Response Structure:**
```
Byte 0:    0x0B   - Mode data length (11 bytes following)
Byte 1:    0x00   - Medium type (direct access)
Byte 2:    0x00   - Device-specific parameter
Byte 3:    0x00   - Block descriptor length (no descriptor)

Caching Mode Page (Page Code 0x08):
Byte 4:    0x08   - Page code (Caching parameters)
Byte 5:    0x06   - Page length (6 bytes)
Byte 6:    0x00   - Flags - WCE=0 (bit 2 clear = DISABLED)
Bytes 7-11: 0x00   - Reserved
```

**MODE SENSE 10 Response Structure:**
```
Bytes 0-1:  0x00 0x0E - Mode data length (14 bytes following)
Byte 2:     0x00      - Medium type
Byte 3:     0x00      - Device-specific parameter
Bytes 4-5:  0x00 0x00 - Reserved
Bytes 6-7:  0x00 0x00 - Block descriptor length (none)

Caching Mode Page (Page Code 0x08):
Byte 8:     0x08      - Page code
Byte 9:     0x06      - Page length
Byte 10:    0x00      - Flags - WCE=0 (DISABLED)
Bytes 11-15: 0x00     - Reserved
```

## Expected Behavior After Update

After flashing this firmware:

1. **All operating systems** (Windows, macOS, Linux) should automatically treat writes as synchronous
2. **No user configuration required** - no O_SYNC flags, no special mount options
3. **Works with all applications** - file managers, command-line tools, scripts
4. **Data persistence guaranteed** - writes complete before OS returns "success"

## Testing Instructions

1. Flash the updated firmware:
   ```bash
   make flash
   ```

2. Run the standard Python test **WITHOUT** any modifications:
   ```bash
   python3 test_data_integrity.py /path/to/device
   ```

3. Both tests should now pass:
   - Test 1: Write and immediate read-back
   - Test 2: Persistence after remount

4. The test should work on ALL operating systems without special flags

## Important Notes

- This change forces **all writes to be synchronous**, which is correct for our device since we don't have a write cache
- The write speed impact is negligible (we're only writing 4KB total)
- This is the **standard USB MSC best practice** for devices without write cache
- Users no longer need to "Safely Eject" before disconnect (though it's still good practice)

## What Changed vs. Before

**Before:**
- OS assumed device had write cache (default behavior)
- OS buffered writes in RAM for performance
- Quick disconnects = data loss

**After:**
- Device explicitly reports "no write cache"
- OS sends all writes synchronously
- Quick disconnects = data safe

## Firmware Verification

After flashing, you can verify the MODE SENSE response using Linux:
```bash
sg_modes -p 0x08 /dev/sdX
```

This should show the Caching mode page with WCE=0.
