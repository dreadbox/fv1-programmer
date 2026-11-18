# Diagnostic Test Results

**Date:** 2026-01-02
**Test:** Pattern write and location verification

## ✅ FIRST DIAGNOSTIC COMPLETE

### Boot Sector Analysis (from `diagnose_fat12.py --analyze`)
```
Bytes per sector:    512
Sectors per cluster: 1
Reserved sectors:    1
Number of FATs:      2
Root entries:        48 ✓
Total sectors:       14
Sectors per FAT:     1
Hidden sectors:      0
Root dir sectors:    3 (calculated) ✓
First data sector:   6 (calculated) ✓
```

**✓ Linux is correctly calculating `first_data_sector = 6`** (matches firmware!)

### Pattern Location Results
```
✓ FOUND pattern at LBA 6 (EEPROM offset 0)   ← EXPECTED LOCATION
✓ FOUND pattern at LBA 7 (EEPROM offset 512) ← Residual from old test?
```

**Analysis:**
- **LBA 6 (offset 0):** This is where we EXPECT the data based on current firmware config
- **LBA 7 (offset 512):** Likely old data from previous test with different `VFAT_DATA_START_SECTOR`

### Why Pattern Appears at Both Locations

**Previous test (Attempt 1 or 2):**
- Firmware had `VFAT_DATA_START_SECTOR = 5`
- Linux calculated `first_data_sector = 6` (adding +1)
- Linux wrote cluster 2 → LBA 6
- Firmware mapped LBA 6 → EEPROM offset `(6-5)*512 = 512`
- Pattern landed at offset 512 ✗

**Current test (Attempt 3):**
- Firmware has `VFAT_DATA_START_SECTOR = 6`
- Linux calculated `first_data_sector = 6` (correct!)
- Linux wrote cluster 2 → LBA 6
- Firmware mapped LBA 6 → EEPROM offset `(6-6)*512 = 0`
- Pattern landed at offset 0 ✓

**The old pattern at offset 512 was never overwritten**, so it still exists!

## 🔍 CONFIRMATION TEST REQUIRED

Since both LBA 6 and LBA 7 contain the same pattern (0x00-0xFF), we cannot definitively determine which is the NEW write vs OLD residual data.

### Verification Test: Write a NEW Distinctive Pattern

To confirm the bug is fixed, write a DIFFERENT pattern (0xAA) and check where it lands:

**Step 1:** Write new pattern
```bash
python3 verify_fix.py
```

**Step 2:** Disconnect and reconnect USB device

**Step 3:** Check where the new pattern landed
```bash
sudo python3 check_new_pattern.py
```

### Expected Results

**If Bug is FIXED:**
```
✓ FOUND 0xAA pattern at LBA 6 (EEPROM offset 0)
✅ BUG IS FIXED!
```
This confirms data is writing to the correct offset.

**If Bug Still Exists:**
```
✓ FOUND 0xAA pattern at LBA 7 (EEPROM offset 512)
❌ BUG STILL EXISTS
```
This means Linux is still miscalculating or firmware mapping is wrong.

## Possible Outcomes

### Outcome A: Pattern at EEPROM Offset 0 (Bug Fixed! 🎉)
```
✓ FOUND pattern at LBA 6 (EEPROM offset 0)
```
**This means:** The 48 root entry fix worked and Linux now correctly calculates `first_data_sector = 6`.

### Outcome B: Pattern at EEPROM Offset 512 (Bug Persists)
```
✓ FOUND pattern at LBA 7 (EEPROM offset 512)
```
**This means:** Linux is still adding +1 to `first_data_sector`, calculating it as 7 instead of 6.

**Next Steps if Bug Persists:**
1. Test **Hypothesis A**: Set hidden sectors = 1 in boot sector
2. Test **Hypothesis D**: Add debug logging to firmware to see actual LBA values
3. Test **Hypothesis B**: Add MBR/partition table
4. **Pragmatic fix**: Adjust `VFAT_DATA_START_SECTOR` to match Linux's behavior

### Outcome C: Pattern at Different Location
If pattern appears elsewhere, this indicates a different type of issue (cluster mapping, FAT encoding, etc.)

## Current Firmware Configuration

```c
// Boot Sector
Root directory entries: 48 (0x30)
Total sectors: 14 (0x0E)
Reserved sectors: 1
Number of FATs: 2
Sectors per FAT: 1

// Virtual Disk Layout
LBA 0:    Boot sector
LBA 1-2:  FAT tables (RAM)
LBA 3-5:  Root directory (RAM, 3 sectors)
LBA 6-13: Data area → EEPROM (4KB)

// Constants
VFAT_DATA_START_SECTOR = 6
VFAT_TOTAL_SECTORS = 14
```

**Expected Linux Calculation:**
```
first_data_sector = 1 + (2 × 1) + 3 = 6
Cluster 2 → LBA = 6 + (2-2) × 1 = 6
EEPROM offset = (6-6) × 512 = 0 ✓
```

## Files Created During This Diagnostic Session

1. `diagnose_fat12.py` - Main diagnostic tool (write pattern + analyze)
2. `verify_pattern_simple.py` - Quick filesystem verification (no sudo needed)
3. `DIAGNOSTIC_RESULTS.md` - This file

## Reference: Understanding the Bug

The sector offset bug occurs when Linux's FAT driver calculates `first_data_sector` differently than our firmware expects:

**Example from Previous Attempts:**
- Firmware configured: `first_data_sector = 5` (with 32 root entries)
- Linux calculated: `first_data_sector = 6` (+1 offset)
- Result: Data written to LBA 6 instead of LBA 5
- EEPROM offset: `(6-5) × 512 = 512` bytes (wrong!)

**If the fix worked with 48 entries:**
- Firmware configured: `first_data_sector = 6`
- Linux calculated: `first_data_sector = 6` (match!)
- Result: Data written to LBA 6
- EEPROM offset: `(6-6) × 512 = 0` (correct!)

## How to Interpret Raw Sector Analysis

When you run `python3 diagnose_fat12.py --analyze`, look for:

```
=== Boot Sector Analysis ===
Root entries:        48
First data sector:   6 (calculated)

Searching for test pattern in all sectors...
✓ FOUND pattern at LBA X (EEPROM offset Y)

=== RESULT ===
Expected first_data_sector: 6
Actual data found at LBA(s): [X]
  LBA X: offset = +N sectors (+N*512 bytes)
```

- If `offset = 0`: Bug is fixed! ✓
- If `offset = +1`: Bug persists, Linux still adding +1
- If `offset = -1` or other: Different issue, requires investigation

## Next Actions

**Immediate (User):**
1. Disconnect/reconnect USB if not already done
2. Run `python3 verify_pattern_simple.py`
3. Run `python3 diagnose_fat12.py --analyze`
4. Report the results

**If Bug Fixed:**
- Celebrate! 🎉
- Test with actual FV1 programs
- Update documentation

**If Bug Persists:**
- Review hypotheses in `/home/saolin/Downloads/CLAUDE_CODE_PROMPT_FAT12_DEBUG.md`
- Implement Hypothesis A (hidden sectors test)
- Consider adding firmware debug logging
- Last resort: Pragmatic fix to match Linux behavior
