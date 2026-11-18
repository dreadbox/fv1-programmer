/**
  ******************************************************************************
  * @file           : ram_disk.h
  * @brief          : Write-through FAT12 disk with direct EEPROM access
  ******************************************************************************
  * @description
  *
  * Simple write-through FAT12 implementation that always shows CURRENT.FV1
  * (4096 bytes) containing the EEPROM contents. Writes are sent directly to
  * EEPROM with minimal RAM buffering.
  *
  ******************************************************************************
  */

#ifndef RAM_DISK_H
#define RAM_DISK_H

#include <stdint.h>
#include <stdbool.h>

// Virtual disk geometry (must match boot sector!)
#define RAMDISK_SECTOR_SIZE         512
#define RAMDISK_TOTAL_SECTORS       12
#define RAMDISK_FAT_START_SECTOR    1
#define RAMDISK_FAT_SECTORS         2       // 2 FAT copies
#define RAMDISK_ROOT_START_SECTOR   3
#define RAMDISK_ROOT_SECTORS        1       // 16 entries = 1 sector
#define RAMDISK_DATA_START_SECTOR   4
#define RAMDISK_DATA_SECTORS        8       // 4KB data area (full EEPROM size)

// Directory entry constants
#define DIR_ENTRY_SIZE              32
#define DIR_ENTRIES_PER_SECTOR      16
#define MAX_ROOT_ENTRIES            16

// File attribute flags
#define ATTR_READ_ONLY              0x01
#define ATTR_HIDDEN                 0x02
#define ATTR_SYSTEM                 0x04
#define ATTR_VOLUME_LABEL           0x08
#define ATTR_DIRECTORY              0x10
#define ATTR_ARCHIVE                0x20
#define ATTR_LONG_NAME              0x0F

// Status flags
typedef enum {
    RAMDISK_STATUS_EMPTY = 0,       // No write in progress
    RAMDISK_STATUS_PENDING,         // Write in progress
    RAMDISK_STATUS_COMMITTED,       // Write completed successfully
    RAMDISK_STATUS_ERROR            // Write error occurred
} RamDisk_Status;

/**
  * @brief  Initialize RAM disk
  * @retval None
  */
void RamDisk_Init(void);

/**
  * @brief  Read a sector from disk (EEPROM or FAT12 structures)
  * @param  lba: Logical Block Address (0-11)
  * @param  buffer: Output buffer (must be at least 512 bytes)
  * @retval 0 on success, -1 on error
  */
int8_t RamDisk_ReadSector(uint32_t lba, uint8_t *buffer);

/**
  * @brief  Write a sector to disk (writes data directly to EEPROM)
  * @param  lba: Logical Block Address (0-11)
  * @param  buffer: Input buffer (512 bytes)
  * @retval 0 on success, -1 on error
  */
int8_t RamDisk_WriteSector(uint32_t lba, const uint8_t *buffer);

/**
  * @brief  Get total capacity in blocks
  * @retval Number of blocks (12)
  */
uint32_t RamDisk_GetSectorCount(void);

/**
  * @brief  Get block size
  * @retval Block size in bytes (512)
  */
uint32_t RamDisk_GetSectorSize(void);

/**
  * @brief  Get current write status
  * @retval Current status (EMPTY, PENDING, COMMITTED, or ERROR)
  */
RamDisk_Status RamDisk_GetStatus(void);

/**
  * @brief  Reset write state
  * @retval None
  */
void RamDisk_Reset(void);

#endif // RAM_DISK_H
