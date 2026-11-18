/**
  ******************************************************************************
  * @file           : eeprom_fv1.h
  * @brief          : Driver for FV1 I2C EEPROM (4KB)
  ******************************************************************************
  * @description
  *
  * This driver provides functions for writing to the FV1's I2C EEPROM.
  * The FV1 EEPROM is connected via I2C2 and used to store FV1 programs.
  *
  * Specifications:
  * - Capacity: 32Kbit (4096 bytes) - similar to 24LC32
  * - I2C Address: 0x60 (7-bit)
  * - Page Size: 32 bytes (assumed, same as 24LC32)
  * - Write Cycle Time: 5ms max
  *
  ******************************************************************************
  */

#ifndef __EEPROM_FV1_H
#define __EEPROM_FV1_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "eeprom_24lc32.h"  // Re-use status codes
#include <stdbool.h>

/* FV1 EEPROM Configuration --------------------------------------------------*/
#define FV1_I2C_HANDLE          hi2c2               // I2C2 handle
#define FV1_EEPROM_ADDRESS      0x50                // 7-bit address (same as 24LC32, but on different bus)
#define FV1_EEPROM_SIZE         4096                // Total size in bytes
#define FV1_EEPROM_PAGE_SIZE    32                  // Page size in bytes
#define FV1_WRITE_TIMEOUT       20                  // Write timeout in ms

/* Function Prototypes -------------------------------------------------------*/

/**
  * @brief  Initialize the FV1 EEPROM driver
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status FV1_EEPROM_Init(void);

/**
  * @brief  Write multiple bytes to FV1 EEPROM (handles page boundaries)
  * @param  addr: Starting memory address
  * @param  buffer: Pointer to data to write
  * @param  size: Number of bytes to write
  * @retval EEPROM_Status: Operation status
  * @note   This function automatically handles 32-byte page boundaries
  */
EEPROM_Status FV1_EEPROM_WriteBuffer(uint16_t addr, uint8_t *buffer, uint16_t size);

/**
  * @brief  Check if FV1 EEPROM is ready (ACK polling)
  * @retval true if ready, false if busy
  */
bool FV1_EEPROM_IsReady(void);

/**
  * @brief  Wait for FV1 EEPROM write cycle to complete
  * @param  timeout_ms: Maximum wait time in milliseconds
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status FV1_EEPROM_WaitReady(uint32_t timeout_ms);

/**
  * @brief  Copy data from 24LC32 EEPROM (I2C1) to FV1 EEPROM (I2C2)
  * @retval EEPROM_Status: Operation status
  * @note   Copies all 4096 bytes from source to destination
  */
EEPROM_Status FV1_EEPROM_CopyFromSource(void);

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_FV1_H */
