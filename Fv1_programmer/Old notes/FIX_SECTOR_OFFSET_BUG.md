# Fix for +1 Sector Offset Bug

## Problem Summary

Data written to file offset 0 appears at EEPROM offset 512 (sector 1) instead of offset 0 (sector 0).

### Root Cause

Linux calculates `first_data_sector = 5` instead of the firmware's expected `4`, causing a +1 sector offset.

**Why?**
- Firmware: `root_dir_sectors = 1` (16 entries × 32 bytes = 512 bytes = 1 sector)
- Linux: `root_dir_sectors = 2` (likely enforces minimum of 2 sectors)

**Calculation:**
```
first_data_sector = reserved_sectors + (num_fats × sectors_per_fat) + root_dir_sectors
                  = 1 + (2 × 1) + root_dir_sectors
```

- Firmware: `1 + 2 + 1 = 4`
- Linux: `1 + 2 + 2 = 5` ← Mismatch!

### Impact

When Linux writes to cluster 2 (start of PROGRAMS.BIN):
- Linux calculates: `LBA = 5 + (2-2)×1 = 5`
- Firmware receives: WRITE to LBA 5
- Firmware stores: `eeprom_addr = (5-4)×512 = 512` ✗ WRONG!
- Should be: `eeprom_addr = 0`

## Solution

Change root directory from 1 sector (16 entries) to 2 sectors (32 entries) to match Linux's expectation.

### Changes Required in `Core/Src/virtual_fat12.c`

#### 1. Update Boot Sector (lines ~42 and ~43)

**Before:**
```c
0x10, 0x00,              // Root directory entries: 16 (0x0010)
0x0C, 0x00,              // Total sectors: 12 (0x000C) = 6KB
```

**After:**
```c
0x20, 0x00,              // Root directory entries: 32 (0x0020)
0x0D, 0x00,              // Total sectors: 13 (0x000D) = 6.5KB
```

#### 2. Extend Root Directory Array (line ~106)

**Before:**
```c
static uint8_t root_directory[512] = {
    // Entry 1: PROGRAMS.BIN file
    'P', 'R', 'O', 'G', 'R', 'A', 'M', 'S',
    // ... (rest of entry)
};
```

**After:**
```c
static uint8_t root_directory[1024] = {
    // Entry 1: PROGRAMS.BIN file (first 32 bytes)
    'P', 'R', 'O', 'G', 'R', 'A', 'M', 'S',
    // ... (rest of entry)

    // Entries 2-32: unused (fill with zeros)
    [32 ... 1023] = 0x00
};
```

#### 3. Update Read Sector Function (line ~174)

**Before:**
```c
case 3:
    // Root directory from RAM
    memcpy(buffer, root_directory, VFAT_SECTOR_SIZE);
    break;

default:
    // Data sectors 4-11: read from EEPROM
```

**After:**
```c
case 3:
case 4:
    // Root directory from RAM (2 sectors)
    {
        uint16_t offset = (lba - 3) * VFAT_SECTOR_SIZE;
        memcpy(buffer, root_directory + offset, VFAT_SECTOR_SIZE);
    }
    break;

default:
    // Data sectors 5-12: read from EEPROM
```

#### 4. Update Write Sector Function (line ~215)

**Before:**
```c
case 3:
    // Root directory is read-only - silently ignore writes
    // PROGRAMS.BIN entry is always present and cannot be deleted
    break;

default:
    // Data sectors 4-11: write to EEPROM
```

**After:**
```c
case 3:
case 4:
    // Root directory is read-only - silently ignore writes
    // PROGRAMS.BIN entry is always present and cannot be deleted
    break;

default:
    // Data sectors 5-12: write to EEPROM
```

### Changes Required in `Core/Inc/virtual_fat12.h`

#### Update Constants (line ~38)

**Before:**
```c
#define VFAT_DATA_START_SECTOR   4      // First data sector
#define VFAT_TOTAL_SECTORS       12     // Total sectors in filesystem
```

**After:**
```c
#define VFAT_DATA_START_SECTOR   5      // First data sector (was 4)
#define VFAT_TOTAL_SECTORS       13     // Total sectors in filesystem (was 12)
```

## New Filesystem Layout

### Before (Broken)
```
LBA  0     : Boot sector
LBA  1     : FAT #1
LBA  2     : FAT #2
LBA  3     : Root directory (1 sector, 16 entries)
LBA  4-11  : Data area → EEPROM offsets 0-3583
```

### After (Fixed)
```
LBA  0     : Boot sector
LBA  1     : FAT #1
LBA  2     : FAT #2
LBA  3-4   : Root directory (2 sectors, 32 entries)
LBA  5-12  : Data area → EEPROM offsets 0-3583
```

## Verification

After making changes:

1. **Build and flash firmware**
2. **Connect device and mount**
3. **Run test:**
   ```bash
   python3 test_sector_sizes.py "/media/saolin/FV1 PROGRAM" 1
   ```
4. **Eject, reconnect, and verify:**
   ```bash
   python3 read_eeprom_raw.py "/media/saolin/FV1 PROGRAM"
   ```

**Expected result:**
- First 16 bytes should be: `00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f`
- No sector offset - data at correct location!

## Why This Fix Works

By changing to 32 root directory entries:
- `root_dir_sectors = ((32 × 32) + 511) / 512 = 1535 / 512 = 2`
- `first_data_sector = 1 + 2 + 2 = 5`

Now both firmware and Linux agree:
- Cluster 2 → LBA 5
- EEPROM offset = (5-5)×512 = 0 ✓ CORRECT!

## Alternative Solutions Considered

### Option 1: Keep 16 entries, work around Linux bug
- Add offset correction in firmware: `eeprom_addr = (lba - 5) * 512`
- **Rejected**: Hacky, fragile, doesn't fix root cause

### Option 2: Use 224 entries (standard floppy)
- Would work but wastes more RAM (7KB vs 1KB)
- **Rejected**: Unnecessarily large for our use case

### Option 3: Report Linux kernel bug
- Might be a real bug in Linux FAT driver for small filesystems
- **Rejected**: Takes too long, we need a fix now

## Conclusion

The recommended fix (32 entries, 2 sectors) is:
- ✓ Simple and clean
- ✓ Matches Linux's expectations
- ✓ Minimal RAM overhead (512 bytes extra)
- ✓ Standards-compliant
- ✓ Works with all operating systems
