# Data Persistence Issue - Systematic Analysis

## What We Know

1. ✓ EEPROM hardware persistence works (boot test confirmed with 3 blinks)
2. ✓ Test 1 passes (write → read-back immediately → checksums match)
3. ✗ Test 2 fails (after reboot → read → checksums don't match)

## Data Flow Analysis

### Write Path (USB → EEPROM)
```
Python writes to /dev/FV1_PROGRAM/PROGRAMS.BIN
  ↓
OS sends USB SCSI Write commands
  ↓
STORAGE_Write_FS(lun, buf, blk_addr=4-11, blk_len=8)
  ↓
VFAT_WriteSector(lba=4-11, buffer=512 bytes each)
  ↓
eeprom_addr = (lba - 4) * 512  [Maps LBA 4→0, 5→512, ..., 11→3584]
  ↓
EEPROM_WriteBuffer(addr, buffer, 512)
  ↓
  For each 32-byte page:
    - HAL_I2C_Master_Transmit([addr_high, addr_low, data...])
    - EEPROM_WaitReady(20ms)
  ↓
Additional EEPROM_WaitReady(100ms) after sector completes
```

### Read Path (EEPROM → USB)
```
Python reads /dev/FV1_PROGRAM/PROGRAMS.BIN
  ↓
OS sends USB SCSI Read commands
  ↓
STORAGE_Read_FS(lun, buf, blk_addr=4-11, blk_len=8)
  ↓
VFAT_ReadSector(lba=4-11, buffer)
  ↓
eeprom_addr = (lba - 4) * 512
  ↓
EEPROM_ReadBuffer(addr, buffer, 512)
  ↓
HAL_I2C_Master_Transmit([addr_high, addr_low])
HAL_I2C_Master_Receive(buffer, 512)
```

## FAT12 Structure Analysis

### Boot Sector (LBA 0) - in Flash
- Sectors per cluster: 1 (each cluster = 512 bytes)
- Total sectors: 12
- Reserved sectors: 1 (boot sector)
- FATs: 2 copies, 1 sector each
- Root directory: 1 sector (16 entries × 32 bytes)
- Data area starts at LBA 4

### Root Directory (LBA 3) - in RAM
- Entry 0: Volume label "FV1 PROGRAM"
- Entry 1: PROGRAMS.BIN
  - Start cluster: 2 (little-endian: 0x02, 0x00)
  - File size: 4096 bytes (little-endian: 0x00, 0x10, 0x00, 0x00)
  - Attributes: 0x20 (Archive)

### FAT Table (LBA 1-2) - in RAM
- Cluster 0: 0xFF0 (media descriptor)
- Cluster 1: 0xFFF (reserved)
- Cluster 2→3→4→5→6→7→8→9→EOF (0xFFF)
- 8 clusters total = 8 sectors = 4096 bytes ✓

### Cluster to Sector Mapping
```
Cluster 2 → LBA 4  → EEPROM 0-511
Cluster 3 → LBA 5  → EEPROM 512-1023
Cluster 4 → LBA 6  → EEPROM 1024-1535
Cluster 5 → LBA 7  → EEPROM 1536-2047
Cluster 6 → LBA 8  → EEPROM 2048-2559
Cluster 7 → LBA 9  → EEPROM 2560-3071
Cluster 8 → LBA 10 → EEPROM 3072-3583
Cluster 9 → LBA 11 → EEPROM 3584-4095
```
Mapping appears correct ✓

## Possible Issues

### Theory 1: OS Write Caching
- OS might not send all writes before eject
- os.sync() might not work properly for USB devices
- **Test**: Add explicit fsync() in Python script

### Theory 2: FAT/Directory RAM Reset
- FAT and directory are static arrays in RAM
- On reboot, they're re-initialized to defaults
- **Issue**: If OS modified them during write, changes are lost
- **But**: We return success even when ignoring writes, so OS thinks it succeeded
- **Concern**: OS might behave differently on read if FAT/directory don't match expectations

### Theory 3: Incomplete EEPROM Writes
- Write might return success but internal write cycle not complete
- 100ms wait might not be enough for all 16 pages × 8 sectors = 128 pages
- **But**: Boot test (1 page) works fine
- **Test**: Increase timeout significantly

### Theory 4: LBA Mapping Bug
- Arithmetic looks correct but might have edge case
- **Test**: Write known pattern sector-by-sector and verify

### Theory 5: OS File Handling
- OS might write to different sectors than expected
- Might not write all sectors if file already exists
- **Test**: Monitor which LBAs are actually written

### Theory 6: Read-Only FAT/Directory Confusion
- OS updates FAT/directory during write
- We silently ignore updates
- On reboot, FAT/directory reset to defaults
- OS might be confused when reading
- **But**: FAT chain is the same, so shouldn't matter

## Critical Question
If Test 1 passes (immediate read-back works), where is the data being read from?
- Option A: From EEPROM (our assumption)
- Option B: From OS cache (would explain why Test 2 fails)

If Option B, then EEPROM writes might not be happening at all!

## Next Diagnostic Step

Create firmware that PROVES data flow:
1. On boot, write incrementing pattern to EEPROM (0x00, 0x01, 0x02, ...)
2. Start USB
3. User reads PROGRAMS.BIN via USB
4. Verify contents match the incrementing pattern

This confirms:
- EEPROM write at boot works
- USB read from EEPROM works
- Mapping is correct

Then:
5. User writes different pattern via USB
6. Reboot
7. Check if new pattern is in EEPROM

This isolates whether USB writes reach EEPROM at all.
