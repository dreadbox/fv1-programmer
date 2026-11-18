# Data Integrity Test Instructions

## Overview
This test verifies that your FV1 programmer correctly writes data to EEPROM without corruption and that data persists across reboots.

## Prerequisites
1. Build and flash the firmware to your STM32 device
2. Connect the device via USB to your computer
3. Ensure the write-protect switch is in the UNLOCKED position (LED should be ON)
4. Python 3 installed on your system

## Running the Test

### Step 1: Find the device mount path

**On Linux:**
```bash
# The device will typically mount at:
ls /media/$USER/
# Look for "FV1_PROGRAM" or similar
```

**On macOS:**
```bash
# The device will typically mount at:
ls /Volumes/
# Look for "FV1_PROGRAM"
```

### Step 2: Run the test script

**On Linux:**
```bash
python3 test_data_integrity.py /media/$USER/FV1_PROGRAM
```

**On macOS:**
```bash
python3 test_data_integrity.py /Volumes/FV1_PROGRAM
```

### Step 3: Follow the on-screen instructions

The test runs in two phases:

**Phase 1: Write and Immediate Read-back**
- Generates 4096 bytes of test data with known patterns
- Writes to PROGRAMS.BIN
- Reads back immediately
- Verifies checksum matches

**Phase 2: Persistence Test**
- You'll be prompted to:
  1. Safely eject/unmount the device
  2. Disconnect USB cable
  3. Wait 2 seconds
  4. Reconnect USB cable
  5. Remount to the same path
  6. Press Enter to continue
- Script reads the file again
- Verifies data is still intact

## Expected Output

### Success (All Tests Pass):
```
============================================================
TEST SUMMARY
============================================================
Test 1 (Write & Read-back): ✓ PASSED
Test 2 (Persistence):       ✓ PASSED

✓ ALL TESTS PASSED!

Your FV1 programmer is working correctly:
  - Data writes without corruption
  - Data reads back accurately
  - Data persists across power cycles

You can now proceed to Phase 4 (FV1 programming)
============================================================
```

### Failure Examples:

**Test 1 Failure (Data Corruption):**
```
✗ FAILED: Checksum mismatch!
  First difference at byte 512:
    Expected: 0x00
    Got:      0xFF
```
Possible causes:
- EEPROM write timeout issues
- I2C communication errors
- Virtual FAT sector mapping errors

**Test 2 Failure (No Persistence):**
```
✗ TEST 2 FAILED - Data not persistent!
```
Possible causes:
- EEPROM writes not completing before USB disconnect
- ACK polling timeout too short
- Write protection issue

## Troubleshooting

### "Device path does not exist"
- Verify the device is mounted
- Check the exact mount path (case-sensitive)
- On macOS, use `/Volumes/FV1_PROGRAM` not `/Volumes/FV1 PROGRAM`

### "Permission denied" when writing
- Ensure write-protect switch is UNLOCKED (LED ON)
- Check file permissions on the mount point
- Try running with sudo (not recommended, but may help diagnose)

### Device unmounts during test
- This should NOT happen with the fixed firmware
- If it does, check that all HAL_Delay() calls were removed from USB callbacks
- Review USB_DEVICE/App/usbd_storage_if.c and Core/Src/virtual_fat12.c

### LED behavior during test
- LED should flicker rapidly during writes
- This confirms EEPROM write operations are happening
- If LED stays solid or off, writes may not be reaching EEPROM layer

## What the Test Data Contains

The test generates 4096 bytes with multiple patterns:
- **Bytes 0-511:** Incrementing sequence (0x00, 0x01, 0x02, ... 0xFF, 0x00, ...)
- **Bytes 512-1023:** Decrementing sequence (0xFF, 0xFE, 0xFD, ...)
- **Bytes 1024-2047:** Alternating 0xAA, 0x55 pattern
- **Bytes 2048-3071:** Pseudo-random (fixed seed for reproducibility)
- **Bytes 3072-4095:** Repeating 0-255 sequence

This variety helps detect:
- Bit flips
- Byte swapping
- Page boundary errors
- Sequential write issues

## Next Steps After Passing

Once both tests pass, you can confidently proceed to:
1. **Phase 4:** Implement FV1 EEPROM programming via I2C2
2. Real-world usage: Program actual FV1 DSP chips
3. Create FV1 program files and test them

## Questions or Issues?

If tests fail consistently:
1. Double-check all code changes were applied correctly
2. Verify EEPROM_WaitReady() has NO HAL_Delay() calls
3. Verify VFAT_WriteSector() has NO HAL_Delay() calls
4. Check I2C1 connections to 24LC32 EEPROM
5. Test EEPROM separately with a simple read/write test
