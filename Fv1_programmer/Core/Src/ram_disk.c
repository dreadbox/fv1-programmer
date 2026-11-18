/**
  ******************************************************************************
  * @file           : ram_disk.c
  * @brief          : Write-through FAT12 disk with direct EEPROM access
  ******************************************************************************
  * @description
  *
  * Simple write-through FAT12 implementation:
  * - Always shows "CURRENT.FV1" (4096 bytes) with EEPROM contents
  * - Writes are parsed and sent directly to EEPROM (no RAM buffering)
  * - Minimal RAM usage (~100 bytes for state tracking)
  *
  ******************************************************************************
  */

#include "ram_disk.h"
#include "eeprom_24lc32.h"
#include <string.h>

// ============================================================================
// Boot Sector (stored in Flash as const)
// ============================================================================
static const uint8_t boot_sector[512] = {
    // Jump instruction
    0xEB, 0x3C, 0x90,

    // OEM Name (8 bytes)
    'M', 'S', 'D', 'O', 'S', '5', '.', '0',

    // BIOS Parameter Block
    0x00, 0x02,             // [11-12] Bytes per sector: 512
    0x01,                   // [13] Sectors per cluster: 1
    0x01, 0x00,             // [14-15] Reserved sectors: 1
    0x02,                   // [16] Number of FATs: 2
    0x10, 0x00,             // [17-18] Root entries: 16
    0x0C, 0x00,             // [19-20] Total sectors: 12
    0xF0,                   // [21] Media type: removable
    0x01, 0x00,             // [22-23] Sectors per FAT: 1
    0x01, 0x00,             // [24-25] Sectors per track: 1
    0x01, 0x00,             // [26-27] Number of heads: 1
    0x00, 0x00, 0x00, 0x00, // [28-31] Hidden sectors: 0
    0x00, 0x00, 0x00, 0x00, // [32-35] Large sector count: 0

    // Extended Boot Record
    0x00,                   // [36] Drive number
    0x00,                   // [37] Reserved
    0x29,                   // [38] Boot signature
    0x78, 0x56, 0x34, 0x12, // [39-42] Volume serial number

    // Volume label (11 bytes)
    'F', 'V', '1', ' ', 'P', 'R', 'O', 'G', 'R', 'A', 'M',

    // Filesystem type (8 bytes)
    'F', 'A', 'T', '1', '2', ' ', ' ', ' ',

    // Boot code (448 bytes) - zeros
    [62 ... 509] = 0x00,

    // Boot signature
    [510] = 0x55,
    [511] = 0xAA
};

// ============================================================================
// FAT Table (writable in RAM to track OS updates)
// ============================================================================
// FAT12 cluster chain for CURRENT.FV1 (4096 bytes = 8 clusters):
// Cluster 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → EOF
static uint8_t fat_table[512];

// ============================================================================
// Root Directory Entry (writable in RAM to track OS updates)
// ============================================================================
static uint8_t root_directory[512];

// ============================================================================
// Write State Tracking (minimal RAM usage)
// ============================================================================
typedef struct {
    bool write_in_progress;
    uint32_t bytes_written;
    bool write_error;
} WriteState;

static WriteState write_state = {0};

// ============================================================================
// Initialization
// ============================================================================

void RamDisk_Init(void)
{
    memset(&write_state, 0, sizeof(write_state));

    // Initialize FAT table with CURRENT.FV1 cluster chain
    memset(fat_table, 0, sizeof(fat_table));

    // Entry 0: Media descriptor (0xFF0)
    fat_table[0] = 0xF0;
    fat_table[1] = 0xFF;

    // Entry 1: End-of-chain marker (0xFFF)
    fat_table[2] = 0xFF;

    // Entry 2→3: 0x003 (cluster 2 points to cluster 3)
    fat_table[3] = 0x03;
    fat_table[4] = 0x40;  // Packed: low 4 bits of cluster 3 (0x004)

    // Entry 4→5: 0x005
    fat_table[6] = 0x05;
    fat_table[7] = 0x60;  // Packed: low 4 bits of cluster 5 (0x006)

    // Entry 6→7: 0x007
    fat_table[9] = 0x07;
    fat_table[10] = 0x80; // Packed: low 4 bits of cluster 7 (0x008)

    // Entry 8→9: 0x009, then 9→EOF: 0xFFF
    fat_table[12] = 0x09;
    fat_table[13] = 0xFF; // Packed: cluster 9 = 0xFFF (EOF)

    // Initialize root directory with volume label and CURRENT.FV1
    memset(root_directory, 0, sizeof(root_directory));

    // Entry 0: Volume label
    memcpy(&root_directory[0], "FV1 PROGRAM", 11);
    root_directory[11] = ATTR_VOLUME_LABEL;

    // Entry 1: CURRENT.FV1 file
    memcpy(&root_directory[32], "CURRENT FV1", 11);
    root_directory[43] = ATTR_ARCHIVE;           // Writable
    root_directory[58] = 0x02;                   // Start cluster: 2
    root_directory[60] = 0x00;                   // File size: 4096 bytes
    root_directory[61] = 0x10;                   // (0x1000 in little-endian)
}

// ============================================================================
// USB MSC Interface - Read Sector
// ============================================================================

int8_t RamDisk_ReadSector(uint32_t lba, uint8_t *buffer)
{
    if (buffer == NULL || lba >= RAMDISK_TOTAL_SECTORS) {
        return -1;
    }

    switch (lba) {
        case 0:
            // Boot sector
            memcpy(buffer, boot_sector, RAMDISK_SECTOR_SIZE);
            break;

        case 1:
        case 2:
            // FAT tables (both identical)
            memcpy(buffer, fat_table, RAMDISK_SECTOR_SIZE);
            break;

        case 3:
            // Root directory
            memcpy(buffer, root_directory, RAMDISK_SECTOR_SIZE);
            break;

        default:
            // Data sectors (LBA 4-11) - read from EEPROM
            if (lba >= RAMDISK_DATA_START_SECTOR &&
                lba < RAMDISK_DATA_START_SECTOR + RAMDISK_DATA_SECTORS) {

                uint32_t eeprom_offset = (lba - RAMDISK_DATA_START_SECTOR) * RAMDISK_SECTOR_SIZE;

                EEPROM_Status status = EEPROM_ReadBuffer(eeprom_offset, buffer, RAMDISK_SECTOR_SIZE);
                if (status != EEPROM_OK) {
                    return -1;
                }
            } else {
                return -1;
            }
            break;
    }

    return 0;
}

// ============================================================================
// USB MSC Interface - Write Sector
// ============================================================================

int8_t RamDisk_WriteSector(uint32_t lba, const uint8_t *buffer)
{
    if (buffer == NULL || lba >= RAMDISK_TOTAL_SECTORS) {
        return -1;
    }

    switch (lba) {
        case 0:
            // Boot sector - ignore writes (read-only)
            break;

        case 1:
        case 2:
            // FAT tables - update buffer so OS can track free clusters
            memcpy(fat_table, buffer, RAMDISK_SECTOR_SIZE);
            break;

        case 3:
            // Root directory - update buffer so OS can create/delete files
            memcpy(root_directory, buffer, RAMDISK_SECTOR_SIZE);
            break;

        default:
            // Data sectors (LBA 4-11) - write directly to EEPROM
            if (lba >= RAMDISK_DATA_START_SECTOR &&
                lba < RAMDISK_DATA_START_SECTOR + RAMDISK_DATA_SECTORS) {

                uint32_t eeprom_offset = (lba - RAMDISK_DATA_START_SECTOR) * RAMDISK_SECTOR_SIZE;

                write_state.write_in_progress = true;

                EEPROM_Status status = EEPROM_WriteBuffer(eeprom_offset, (uint8_t*)buffer, RAMDISK_SECTOR_SIZE);

                if (status == EEPROM_OK) {
                    write_state.bytes_written += RAMDISK_SECTOR_SIZE;
                } else {
                    write_state.write_error = true;
                    return -1;
                }
            } else {
                return -1;
            }
            break;
    }

    return 0;
}

// ============================================================================
// Capacity Functions
// ============================================================================

uint32_t RamDisk_GetSectorCount(void)
{
    return RAMDISK_TOTAL_SECTORS;
}

uint32_t RamDisk_GetSectorSize(void)
{
    return RAMDISK_SECTOR_SIZE;
}

// ============================================================================
// Status Functions
// ============================================================================

RamDisk_Status RamDisk_GetStatus(void)
{
    if (write_state.write_error) {
        return RAMDISK_STATUS_ERROR;
    }

    if (write_state.write_in_progress) {
        if (write_state.bytes_written >= 4096) {
            return RAMDISK_STATUS_COMMITTED;
        }
        return RAMDISK_STATUS_PENDING;
    }

    return RAMDISK_STATUS_EMPTY;
}

void RamDisk_Reset(void)
{
    RamDisk_Init();
}
