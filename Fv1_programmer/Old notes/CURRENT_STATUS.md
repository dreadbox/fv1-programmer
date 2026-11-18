# Current Status - Sector Offset Bug

**Last Updated:** 2026-01-02 (Awaiting Diagnostic Results)

## Problem Summary

Data written to `PROGRAMS.BIN` file offset 0 consistently appears at EEPROM offset 512 (sector 1) instead of offset 0, regardless of firmware configuration changes.

## What We've Tried

### ❌ Attempt 1: 16 Root Entries → 32 Entries
- Linux calculated `first_data_sector = 6` (expected 5)
- Data still offset by +512 bytes

### ❌ Attempt 2: 32 Root Entries → 48 Entries
- Linux calculated `first_data_sector = 7` (expected 6)
- Data still offset (test failed)

### 🔍 Attempt 3: 48 Root Entries (Still Testing)
**Status:** Diagnostic test in progress

**Changes Made:**
- Boot sector: Root entries 48 (0x30)
- Boot sector: Total sectors 14 (0x0E)
- Root directory: 3 sectors (1536 bytes)
- `VFAT_DATA_START_SECTOR = 6`
- `VFAT_TOTAL_SECTORS = 14`

**Test Pattern Written:**
- Pattern: `0x00-0xFF` repeated twice (512 bytes)
- ✓ Filesystem view shows pattern at offset 0
- ⚠ Need to verify EEPROM persistence after disconnect/reconnect

**AWAITING USER ACTION:**
1. Disconnect/reconnect USB device (if not done)
2. Run: `python3 diagnose_fat12.py --analyze`
3. Check where pattern actually landed in EEPROM

## Key Finding

**Linux consistently adds +1 to `first_data_sector` regardless of what we configure in the boot sector.**

This suggests the issue is NOT with our root directory size calculations, but something else entirely.

## Current Firmware Configuration

```
Boot Sector:
- Root directory entries: 48 (0x30)
- Total sectors: 14 (0x0E)
- Reserved sectors: 1
- Number of FATs: 2
- Sectors per FAT: 1

Filesystem Layout:
- LBA 0: Boot sector
- LBA 1-2: FAT tables
- LBA 3-5: Root directory (3 sectors)
- LBA 6-13: Data area → EEPROM

Constants:
- VFAT_DATA_START_SECTOR = 6
- VFAT_TOTAL_SECTORS = 14
```

## Likely Root Causes to Investigate

1. **Linux filesystem cache** - OS might be caching old structure
2. **Partition table expectation** - Linux might expect MBR
3. **Minimum filesystem size** - 14 sectors might be too small
4. **USB MSC geometry mismatch** - SCSI parameters vs boot sector

## Recommended Next Steps

1. Check `dmesg` for FAT warnings
2. Verify boot sector with `dd` command
3. Try rebooting system to clear cache
4. Consider adding MBR/partition table
5. Test with standard floppy size (2880 sectors, 224 root entries)

## Files to Review

- `DEBUGGING_SUMMARY.md` - Complete investigation history
- `diagnose_linux_behavior.py` - Analysis of Linux calculations
- `FIX_SECTOR_OFFSET_BUG.md` - Previous fix attempt documentation
