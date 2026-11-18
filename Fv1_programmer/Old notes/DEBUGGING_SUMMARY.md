# Debugging Summary: Sector Offset Bug

## Timeline of Investigation

### Initial Problem
- Data written to PROGRAMS.BIN with `os.O_SYNC` flag worked correctly
- Data written without `O_SYNC` passed immediate read-back test but failed after device reconnect
- Even with proper "Eject" before disconnect, data appeared scrambled or at wrong offsets

### Test 1: Multi-Sector Write Pattern
**Test:** Write 8 sectors (0-7) with distinct patterns
**Result:**
- Immediate read-back: All 8 sectors CORRECT ✓ (cached)
- After reconnect: Only sectors 3 and 7 CORRECT
- Other sectors showed circular shift/rotation pattern

**Conclusion:** Not a caching issue - data was persisted but to wrong locations

### Test 2: Pattern Mapping Analysis
**Test:** Identify which physical EEPROM sectors contain which logical file sectors

**Result:**
```
Physical Sector → Contains Data From → Offset
Sector 0 → Sector 6 → +6
Sector 1 → Sector 0 → -1
Sector 2 → Sector 1 → -1
Sector 3 → Sector 2 → -1 (CORRECT relative position)
Sector 4 → Sector 3 → -1
Sector 5 → Sector 4 → -1
Sector 6 → Sector 5 → -1
Sector 7 → Sector 6 → -1 (CORRECT relative position)
```

**Conclusion:** Systematic sector remapping, not random corruption

### Test 3: FAT12 Encoding Verification
**Test:** Verify FAT table is correctly encoded using FAT12 spec

**Result:**
```
Cluster 0: 0xFF0 (media type) ✓
Cluster 1: 0xFFF (reserved) ✓
Cluster 2: 0x003 → 3 ✓
Cluster 3: 0x004 → 4 ✓
... (all clusters correct)
Cluster 9: 0xFFF → EOF ✓
```

**Conclusion:** FAT encoding is 100% correct - not the cause

### Test 4: Single Sector Write
**Test:** Write only 512 bytes (1 sector) with incrementing pattern (0x00 0x01 0x02...)

**Result:**
- Immediate read-back: CORRECT ✓
- After eject/reconnect: Data NOT at offset 0
- EEPROM still contained OLD data (0x20 pattern) at offset 0

**Conclusion:** Even single-sector write fails - rules out multi-sector buffering issues

### Test 5: Pattern Location Search
**Test:** Search entire EEPROM for the missing data pattern

**Critical Finding:**
```
✓ Found 'Test 1 pattern (0x00 0x01 0x02...)' at 1 location:
  Offset  512 (sector 1, byte 0): 00 01 02 03 04 05 06 07...
```

**Conclusion:**
- File offset 0 → EEPROM offset 512
- This is a **+1 sector offset bug**
- Firmware expected write to LBA 4, but OS sent write to LBA 5

### Test 6: Filesystem Structure Analysis
**Test:** Calculate expected vs actual sector mappings

**Firmware's Expectation:**
```
first_data_sector = reserved_sectors + (num_fats × sectors_per_fat) + root_dir_sectors
                  = 1 + (2 × 1) + 1
                  = 4

Cluster 2 → LBA = first_data_sector + (2-2)×1 = 4
           → EEPROM offset = (4-4)×512 = 0 ✓
```

**Linux's Calculation:**
```
first_data_sector = 1 + (2 × 1) + 2 = 5  (assumes root_dir_sectors = 2)

Cluster 2 → LBA = 5 + (2-2)×1 = 5
           → EEPROM offset = (5-4)×512 = 512 ✗ WRONG!
```

**Reverse Calculation from Observed Behavior:**
```
EEPROM offset 512 → LBA 5 (since (5-4)×512 = 512)
LBA 5 → Cluster 3 (since 5 = first_data_sector + (3-2)×1)
       OR
       first_data_sector = 5 (if cluster = 2)
```

**Conclusion:**
- Linux thinks `first_data_sector = 5` instead of `4`
- This means Linux calculates `root_dir_sectors = 2` instead of `1`

### Test 7: Boot Sector Verification
**Test:** Verify all boot sector fields are correctly encoded (little-endian)

**Result:**
```
Offset 11-12: Bytes per sector    = 512  ✓
Offset 13:    Sectors per cluster = 1    ✓
Offset 14-15: Reserved sectors    = 1    ✓
Offset 16:    Number of FATs      = 2    ✓
Offset 17-18: Root entries        = 16   ✓
Offset 19-20: Total sectors       = 12   ✓
Offset 22-23: Sectors per FAT     = 1    ✓
```

**Conclusion:** Boot sector is 100% correctly encoded

### Test 8: Root Directory Size Analysis
**Test:** Calculate root directory sectors using FAT spec formula

**Formula:**
```
root_dir_sectors = ((RootEntCnt × 32) + (BytsPerSec - 1)) / BytsPerSec
                 = ((16 × 32) + (512 - 1)) / 512
                 = (512 + 511) / 512
                 = 1023 / 512
                 = 1 (integer division)
```

**But Linux calculates:** `root_dir_sectors = 2`

**Possible Causes:**
1. **Minimum root directory size enforcement** - Linux might require minimum 2 sectors
2. **Rounding bug** - Linux might round 16 entries up internally
3. **Alignment requirement** - Linux might require sector alignment
4. **Legacy compatibility** - Standard floppies use 112-224 entries, never 16

**Conclusion:**
- 16 root directory entries is technically valid but unusual
- Linux likely enforces minimum of 32 entries (2 sectors)
- This is why Linux calculates `first_data_sector = 5`

## Root Cause Identified

**Linux FAT driver assumes minimum 32 root directory entries (2 sectors), even though firmware declares 16 entries.**

This causes:
1. Linux calculates `root_dir_sectors = 2` (ignoring the 16 entry declaration)
2. Linux calculates `first_data_sector = 1 + 2 + 2 = 5`
3. Linux maps cluster 2 → LBA 5
4. Firmware receives WRITE to LBA 5
5. Firmware stores to EEPROM offset `(5-4)×512 = 512` instead of `0`

## Why O_SYNC Appeared to Work

The `O_SYNC` flag forces synchronous writes and bypasses normal caching. This likely triggered different code paths in the Linux FAT driver that:
- Performed additional validation
- Used different sector calculation logic
- Or coincidentally wrote to the correct sectors

This masked the underlying sector mapping bug.

## The Fix

Change root directory from 16 entries (1 sector) to 32 entries (2 sectors):

1. Update boot sector: `0x10,0x00` → `0x20,0x00` (root entries)
2. Update boot sector: `0x0C,0x00` → `0x0D,0x00` (total sectors)
3. Extend root_directory array: 512 → 1024 bytes
4. Update `VFAT_DATA_START_SECTOR`: 4 → 5
5. Update `VFAT_TOTAL_SECTORS`: 12 → 13
6. Update read/write sector functions to handle 2-sector root directory

**Result:**
- Firmware: `first_data_sector = 1 + 2 + 2 = 5`
- Linux: `first_data_sector = 1 + 2 + 2 = 5`
- **Both agree!** ✓

Cluster 2 → LBA 5 → EEPROM offset `(5-5)×512 = 0` ✓ CORRECT!

## Lessons Learned

1. **Never assume OS behavior matches specification exactly** - OS implementations often have quirks, minimums, or legacy compatibility requirements

2. **Test with minimal test cases** - The single-sector write test was crucial for isolating the bug

3. **Work backwards from observed behavior** - Finding the data at offset 512 allowed reverse calculation of what Linux was doing

4. **Verify ALL assumptions** - We verified FAT encoding, boot sector encoding, and filesystem calculations step by step

5. **Common doesn't mean required** - While 16 entries is technically valid, standard floppies use 112-224 entries. Linux may optimize for the common case.

## Files Created During Investigation

1. `diagnostic_sector_boundary.py` - Multi-sector write test
2. `analyze_eeprom_dump.py` - Sector mapping analysis
3. `verify_fat_encoding.py` - FAT12 encoding verification
4. `test_sector_sizes.py` - Single/multi-sector write tests
5. `search_pattern.py` - Pattern location search
6. `check_filesystem_structure.py` - Filesystem layout analysis
7. `dump_boot_sector.py` - Boot sector dump instructions
8. `verify_boot_sector_bytes.py` - Boot sector byte verification
9. `analyze_root_dir_size.py` - Root directory size analysis
10. `diagnose_linux_behavior.py` - Linux filesystem behavior analysis
11. `FIX_SECTOR_OFFSET_BUG.md` - Detailed fix instructions
12. `DEBUGGING_SUMMARY.md` - This document

## Update: Fix Attempts Failed

### Attempt 1: 32 Root Directory Entries (2 Sectors)
**Date:** 2026-01-02

**Changes Made:**
- Boot sector: Root entries 16 → 32 (0x20)
- Boot sector: Total sectors 12 → 13 (0x0D)
- Root directory array: 512 → 1024 bytes
- `VFAT_DATA_START_SECTOR`: 4 → 5
- `VFAT_TOTAL_SECTORS`: 12 → 13
- Updated read/write functions for LBA 3-4 (2 root sectors)

**Expected Behavior:**
- Firmware: `first_data_sector = 5` (1 + 2 + 2 = 5)
- Linux: `first_data_sector = 5` (matching firmware)
- Data should write to EEPROM offset 0

**Actual Result:**
- Data still appeared at EEPROM offset 512 (LBA 6)
- Linux wrote cluster 2 to LBA 6 (not LBA 5)

**Analysis:**
- Reverse calculation: LBA 6 - 2 + 2 = `first_data_sector = 6`
- Linux calculated: `root_dir_sectors = 3` (not 2!)
- Formula: `6 = 1 + 2*1 + 3` ✓ Matches

**Conclusion:** Linux enforces minimum of 3 root directory sectors (48 entries), not 2.

### Attempt 2: 48 Root Directory Entries (3 Sectors)
**Date:** 2026-01-02

**Changes Made:**
- Boot sector: Root entries 32 → 48 (0x30)
- Boot sector: Total sectors 13 → 14 (0x0E)
- Root directory array: 1024 → 1536 bytes
- `VFAT_DATA_START_SECTOR`: 5 → 6
- `VFAT_TOTAL_SECTORS`: 13 → 14
- Updated read/write functions for LBA 3-5 (3 root sectors)

**Expected Behavior:**
- Firmware: `first_data_sector = 6` (1 + 2 + 3 = 6)
- Linux: `first_data_sector = 6` (matching firmware)
- Data should write to EEPROM offset 0

**Actual Result:**
- **Test failed again** (details not yet provided by user)
- Pattern continues: Linux appears to be calculating a different value

**Current Status:**
- ❌ 16 root entries → Linux calculates first_data_sector = 5 (expects 4)
- ❌ 32 root entries → Linux calculates first_data_sector = 6 (expects 5)
- ❌ 48 root entries → Linux calculates first_data_sector = ? (expects 6)

**Pattern Observed:**
Linux seems to be consistently adding +1 to first_data_sector regardless of the root directory size we configure.

## Pending Investigation

### Possible Root Causes Still to Explore:

1. **Linux Caching Issue**
   - OS might be caching the old filesystem structure
   - May need to force complete remount or system reboot
   - Could try: `sudo umount /dev/sda && sudo mount /dev/sda /media/...`

2. **Hidden Sector or Partition Table Issue**
   - Boot sector offset might not be at LBA 0
   - Linux might expect a partition table (MBR) before the FAT filesystem
   - Hidden sectors field (offset 28-31) is currently 0x00 0x00 0x00 0x00

3. **Media Descriptor Mismatch**
   - Currently using 0xF0 (removable disk)
   - Might need 0xF8 (fixed disk) or different value
   - First FAT entry must match media descriptor

4. **Volume Label Issue**
   - Volume label in boot sector vs root directory
   - Linux might be confused by mismatched labels

5. **Non-Standard Filesystem Size**
   - 14 sectors (7KB) is extremely small for FAT12
   - Linux might have minimum size requirements or special handling
   - Standard floppy: 1.44MB = 2880 sectors

6. **USB Mass Storage Class Quirks**
   - USB MSC might report different geometry than boot sector
   - SCSI Read Capacity might not match boot sector total sectors
   - Mode Sense parameters might conflict

### Next Steps to Consider:

1. **Verify actual boot sector being read by Linux**
   - Use `sudo dd if=/dev/sda bs=512 count=1 | hexdump -C`
   - Confirm Linux is seeing the updated boot sector

2. **Check dmesg for USB/filesystem errors**
   - `dmesg | tail -50`
   - Look for FAT filesystem warnings or errors

3. **Try standard FAT12 parameters**
   - Use 224 root entries (standard 1.44MB floppy)
   - Use 2880 total sectors (pad remaining with zeros)
   - See if Linux behaves differently with "normal" values

4. **Add MBR/Partition Table**
   - Create a proper Master Boot Record
   - Put FAT filesystem at offset 1 sector (LBA 1+)
   - Set hidden sectors accordingly

5. **Test with different OS**
   - Try Windows or macOS to see if same behavior
   - Helps determine if this is Linux-specific

6. **Enable firmware debug logging**
   - Log all SCSI READ/WRITE commands with LBA values
   - Compare what Linux requests vs what firmware expects

## Technical Notes

### FAT12 Root Directory Size Formula
```
root_dir_sectors = ((RootEntCnt × 32) + (BytsPerSec - 1)) / BytsPerSec
```

For our tests:
- 16 entries: `((16 × 32) + 511) / 512 = 1023 / 512 = 1` sector
- 32 entries: `((32 × 32) + 511) / 512 = 1535 / 512 = 2` sectors
- 48 entries: `((48 × 32) + 511) / 512 = 2047 / 512 = 3` sectors

All calculations are correct per FAT specification.

### First Data Sector Formula
```
first_data_sector = ReservedSectors + (NumFATs × SectorsPerFAT) + RootDirSectors
                  = 1 + (2 × 1) + RootDirSectors
                  = 3 + RootDirSectors
```

Expected values:
- With 1 root sector: `first_data_sector = 4`
- With 2 root sectors: `first_data_sector = 5`
- With 3 root sectors: `first_data_sector = 6`

Linux's actual behavior:
- When we configure 4: Linux uses 5 (+1 offset)
- When we configure 5: Linux uses 6 (+1 offset)
- When we configure 6: Linux uses ? (test failed, offset unknown)

**This consistent +1 offset suggests a systematic issue, not a calculation error.**
