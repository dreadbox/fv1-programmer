/**
  ******************************************************************************
  * @file           : eeprom_24lc32.h
  * @brief          : Driver for 24LC32AT I2C EEPROM (4KB)
  ******************************************************************************
  * @description
  *
  * This driver provides functions for reading and writing to the 24LC32AT
  * I2C EEPROM chip (4096 bytes capacity).
  *
  * Specifications:
  * - Capacity: 32Kbit (4096 bytes)
  * - I2C Address: 0x50 (7-bit, configurable via A0-A2 pins)
  * - Page Size: 32 bytes
  * - Write Cycle Time: 5ms max
  * - Operating Voltage: 2.5V - 5.5V
  *
  ******************************************************************************
  */

#ifndef __EEPROM_24LC32_H
#define __EEPROM_24LC32_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* EEPROM Configuration ------------------------------------------------------*/
#define EEPROM_I2C_HANDLE       hi2c1               // I2C handle (defined in main.c)
#define EEPROM_I2C_ADDRESS      0x50                // 7-bit address (0xA0 >> 1)
#define EEPROM_SIZE             4096                // Total size in bytes
#define EEPROM_PAGE_SIZE        32                  // Page size in bytes
#define EEPROM_WRITE_TIMEOUT    20                  // Write timeout in ms (increased for reliability)
#define EEPROM_MAX_TRIALS       100                 // Max ACK polling trials

/* Status Codes --------------------------------------------------------------*/
typedef enum {
    EEPROM_OK = 0,          // Operation successful
    EEPROM_ERROR,           // General error
    EEPROM_BUSY,            // EEPROM is busy
    EEPROM_TIMEOUT,         // Operation timeout
    EEPROM_INVALID_PARAM    // Invalid parameter
} EEPROM_Status;

/* Function Prototypes -------------------------------------------------------*/

/**
  * @brief  Initialize the EEPROM driver
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_Init(void);

/**
  * @brief  Read a single byte from EEPROM
  * @param  addr: Memory address (0 to 4095)
  * @param  data: Pointer to store the read byte
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_ReadByte(uint16_t addr, uint8_t *data);

/**
  * @brief  Write a single byte to EEPROM
  * @param  addr: Memory address (0 to 4095)
  * @param  data: Byte to write
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_WriteByte(uint16_t addr, uint8_t data);

/**
  * @brief  Read multiple bytes from EEPROM
  * @param  addr: Starting memory address
  * @param  buffer: Pointer to buffer to store read data
  * @param  size: Number of bytes to read
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_ReadBuffer(uint16_t addr, uint8_t *buffer, uint16_t size);

/**
  * @brief  Write multiple bytes to EEPROM (handles page boundaries)
  * @param  addr: Starting memory address
  * @param  buffer: Pointer to data to write
  * @param  size: Number of bytes to write
  * @retval EEPROM_Status: Operation status
  * @note   This function automatically handles 32-byte page boundaries
  */
EEPROM_Status EEPROM_WriteBuffer(uint16_t addr, uint8_t *buffer, uint16_t size);

/**
  * @brief  Erase entire EEPROM (fill with 0xFF)
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_EraseFull(void);

/**
  * @brief  Check if EEPROM is ready (ACK polling)
  * @retval true if ready, false if busy
  */
bool EEPROM_IsReady(void);

/**
  * @brief  Wait for EEPROM write cycle to complete
  * @param  timeout_ms: Maximum wait time in milliseconds
  * @retval EEPROM_Status: Operation status
  */
EEPROM_Status EEPROM_WaitReady(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* __EEPROM_24LC32_H */
