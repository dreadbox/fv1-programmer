# FV1 Programmer - Testing Guide

## Quick Validation Steps

### 1. **Power On Check**
- ✅ Device powers on
- ✅ LED blinks **2 times quickly** (startup indication)
- ✅ LED starts **slow blinking** (1s on, 1s off) = IDLE state

### 2. **USB Enumeration**
```bash
# Check USB device appears
lsusb | grep "0483"

# Check mass storage mount
mount | grep "FV1 PROGRAM"

# Should show ~6.5KB capacity
~df -h /media/*/FV1*PROGRAM*~
```

Expected output:
```
Filesystem      Size  Used Avail Use% Mounted on
/dev/sdb        5.5K     0  5.5K   0% /media/user/FV1 PROGRAM
```

### 3. **Write Test File**
```bash
# Create a test file (2KB example)
dd if=/dev/zero bs=2048 count=1 of=/tmp/test.bin

# Copy to device
cp /tmp/test.bin /media/*/FV1*PROGRAM*/PROGRAM.BIN

# Sync
sync
```

**Expected LED behavior:**
- LED changes from **slow blink** → **fast blink** (200ms on/off)
- Fast blink = PENDING state (file in RAM, awaiting commit)

### 4. **Button Commit Test**

Press the physical button on the device.

**Expected LED sequence:**
1. **Very fast blink** (50ms on/off) = COMMITTING
2. **Solid ON** for 2 seconds = SUCCESS
3. **Slow blink** returns = IDLE (commit complete)

If LED does **triple blink pattern** = ERROR (commit failed)

### 5. **EEPROM Verification**

After successful commit, the data should be in EEPROM (not visible in virtual disk).

To verify data persists:
```bash
# Unplug USB cable
# Wait 2 seconds
# Replug USB cable
# Device should mount again with EMPTY disk (slow blink LED)
```

The virtual disk is now empty because data was committed to EEPROM.

---

## LED States Reference

| LED Pattern | Meaning | Description |
|-------------|---------|-------------|
| 2 quick blinks | **BOOT** | Device started successfully |
| Slow blink (1s on/off) | **IDLE** | No pending file, ready to receive |
| Fast blink (200ms on/off) | **PENDING** | File received, awaiting commit |
| Very fast blink (50ms on/off) | **COMMITTING** | Writing to EEPROM |
| Solid ON (2 sec) | **SUCCESS** | Commit successful |
| Triple blink pattern | **ERROR** | Commit failed |

---

## Automated Testing

Run the comprehensive validation script:

```bash
cd /home/saolin/STM32_EXP/Nymphes_fv1_programmer/Fv1_programmer
./validate_firmware.sh
```

This will test:
- USB device detection
- Mass storage enumeration
- FAT12 filesystem structure
- File write operations
- LED state transitions
- Button commit functionality
- Power cycle persistence
- Maximum file size (4KB)

---

## Common Issues & Solutions

### Issue: Device not detected
- Check USB cable connection
- Verify power LED is on
- Check `dmesg` for enumeration errors:
  ```bash
  sudo dmesg | tail -50
  ```

### Issue: Mass storage not mounting
- Check write-protect switch is in UNLOCKED position
- Try manual mount:
  ```bash
  sudo mount /dev/sdX /mnt
  ```

### Issue: LED shows ERROR (triple blink)
Possible causes:
- No valid file found in root directory
- FAT12 structure corrupted
- EEPROM I2C communication error
- File size invalid (0 bytes or > 4KB)

Check kernel logs:
```bash
sudo dmesg | grep -i "usb\|scsi\|sd"
```

### Issue: Button does nothing
- Ensure switch is in UNLOCKED position (write-protect OFF)
- Ensure a file was written (LED should be fast blinking)
- Check button connection (PB11 on schematic)

---

## File Size Limits

| Limit | Value |
|-------|-------|
| Maximum file size | 4096 bytes (4KB) |
| Minimum file size | 1 byte |
| Virtual disk size | 6656 bytes (6.5KB) |
| Data area | 4096 bytes (8 sectors) |

Files larger than 4KB will be **truncated** to 4KB during commit.

---

## Architecture Verification

The new architecture works as follows:

1. **USB Write** → Data goes to **RAM buffer** (not EEPROM)
2. **LED Fast Blink** → File is pending in RAM
3. **Button Press** → Firmware parses FAT12, extracts file data
4. **LED Very Fast** → Writing to EEPROM via I2C
5. **LED Solid** → Success! Data now in EEPROM
6. **Power Cycle** → RAM cleared, virtual disk empty
7. **EEPROM** → Data persists permanently

This completely avoids the sector offset bug because:
- FAT12 is only used in RAM (never touches EEPROM)
- Button commit extracts raw data from FAT12 clusters
- Raw data writes to EEPROM starting at offset 0
- No filesystem on EEPROM = no offset confusion

---

## Manual EEPROM Read (Advanced)

If you have I2C tools and hardware access:

```bash
# Install i2c-tools
sudo apt-get install i2c-tools

# List I2C buses
i2cdetect -l

# Scan for EEPROM (usually 0x50)
sudo i2cdetect -y 1

# Read first byte
sudo i2cget -y 1 0x50 0x00

# Dump first 256 bytes
sudo i2cdump -y 1 0x50
```

You should see your file data starting at address 0x00.

---

## Performance Metrics

| Operation | Time |
|-----------|------|
| USB file write (4KB) | ~500ms (OS dependent) |
| Button commit (4KB) | ~200ms (EEPROM write time) |
| Power-on boot | ~100ms |
| LED state update | 10ms loop |

---

## Next Steps After Validation

Once all tests pass:

1. Test with real FV1 program files (.bin format)
2. Verify FV1 chip programs correctly from EEPROM
3. Test different file sizes (512B, 1KB, 2KB, 4KB)
4. Test rapid file write → commit cycles
5. Long-term power cycle testing (data retention)

---

## Success Criteria

✅ All of these should work:

- [x] Device enumerates as USB mass storage
- [x] Disk shows 6.5KB capacity
- [x] Files can be copied to the disk
- [x] LED shows fast blink after file write
- [x] Button press triggers commit
- [x] LED shows success pattern
- [x] Data persists in EEPROM after power cycle
- [x] Virtual disk empties after commit
- [x] 4KB files can be written and committed

If all criteria pass: **Firmware is working correctly!** 🎉
