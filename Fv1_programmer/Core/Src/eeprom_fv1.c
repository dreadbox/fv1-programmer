/**
  ******************************************************************************
  * @file           : eeprom_fv1.c
  * @brief          : Driver implementation for FV1 I2C EEPROM
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "eeprom_fv1.h"
#include "eeprom_24lc32.h"

/* External Variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c2;

/* Private Defines -----------------------------------------------------------*/
#define COPY_BUFFER_SIZE  256  // Copy in 256-byte chunks to save RAM

/* Private Functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the FV1 EEPROM driver
  */
EEPROM_Status FV1_EEPROM_Init(void)
{
    // Check if FV1 EEPROM is present by attempting to communicate
    if (HAL_I2C_IsDeviceReady(&FV1_I2C_HANDLE,
                              FV1_EEPROM_ADDRESS << 1,
                              3,
                              100) == HAL_OK)
    {
        return EEPROM_OK;
    }
    return EEPROM_ERROR;
}

/**
  * @brief  Write multiple bytes to FV1 EEPROM (handles page boundaries)
  */
EEPROM_Status FV1_EEPROM_WriteBuffer(uint16_t addr, uint8_t *buffer, uint16_t size)
{
    if (addr >= FV1_EEPROM_SIZE || buffer == NULL || size == 0) {
        return EEPROM_INVALID_PARAM;
    }

    // Check if write would exceed EEPROM size
    if ((uint32_t)addr + size > FV1_EEPROM_SIZE) {
        return EEPROM_INVALID_PARAM;
    }

    uint16_t bytes_written = 0;

    while (bytes_written < size) {
        // Calculate current page and remaining space in page
        uint16_t current_addr = addr + bytes_written;
        uint16_t page_offset = current_addr % FV1_EEPROM_PAGE_SIZE;
        uint16_t bytes_to_page_end = FV1_EEPROM_PAGE_SIZE - page_offset;

        // Calculate how many bytes to write in this iteration
        uint16_t bytes_to_write = (size - bytes_written) < bytes_to_page_end ?
                                  (size - bytes_written) : bytes_to_page_end;

        // Prepare write buffer: [addr_high, addr_low, data...]
        uint8_t write_buffer[FV1_EEPROM_PAGE_SIZE + 2];
        write_buffer[0] = (current_addr >> 8) & 0xFF;
        write_buffer[1] = current_addr & 0xFF;

        // Copy data to write buffer
        for (uint16_t i = 0; i < bytes_to_write; i++) {
            write_buffer[2 + i] = buffer[bytes_written + i];
        }

        // Write page. 50 ms is generous for a single 24LC32-style page
        // (typ. 5 ms tWR). HAL_MAX_DELAY here previously hung the main loop
        // indefinitely when no FV1 target was attached to I2C2.
        if (HAL_I2C_Master_Transmit(&FV1_I2C_HANDLE,
                                    FV1_EEPROM_ADDRESS << 1,
                                    write_buffer,
                                    bytes_to_write + 2,
                                    50) != HAL_OK)
        {
            return EEPROM_ERROR;
        }

        // Wait for write cycle to complete
        if (FV1_EEPROM_WaitReady(FV1_WRITE_TIMEOUT) != EEPROM_OK) {
            return EEPROM_TIMEOUT;
        }

        bytes_written += bytes_to_write;
    }

    return EEPROM_OK;
}

/**
  * @brief  Check if FV1 EEPROM is ready (ACK polling)
  */
bool FV1_EEPROM_IsReady(void)
{
    return (HAL_I2C_IsDeviceReady(&FV1_I2C_HANDLE,
                                  FV1_EEPROM_ADDRESS << 1,
                                  1,
                                  1) == HAL_OK);
}

/**
  * @brief  Wait for FV1 EEPROM write cycle to complete
  */
EEPROM_Status FV1_EEPROM_WaitReady(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms) {
        if (FV1_EEPROM_IsReady()) {
            return EEPROM_OK;
        }
        // Just poll continuously for compatibility
    }

    return EEPROM_TIMEOUT;
}

/**
  * @brief  Copy data from 24LC32 EEPROM (I2C1) to FV1 EEPROM (I2C2)
  * @note   Copies all 4096 bytes in chunks to save RAM
  */
EEPROM_Status FV1_EEPROM_CopyFromSource(void)
{
    uint8_t buffer[COPY_BUFFER_SIZE];
    uint16_t bytes_copied = 0;

    // Fail fast if no FV1 target is attached on I2C2 -- otherwise the first
    // page write would burn its 50 ms timeout and we'd waste several seconds
    // before reporting an error.
    if (HAL_I2C_IsDeviceReady(&FV1_I2C_HANDLE,
                              FV1_EEPROM_ADDRESS << 1,
                              2,
                              50) != HAL_OK) {
        return EEPROM_ERROR;
    }

    // Copy in chunks
    while (bytes_copied < FV1_EEPROM_SIZE) {
        uint16_t bytes_to_copy = (FV1_EEPROM_SIZE - bytes_copied) < COPY_BUFFER_SIZE ?
                                 (FV1_EEPROM_SIZE - bytes_copied) : COPY_BUFFER_SIZE;

        // Read from source EEPROM (24LC32 on I2C1)
        EEPROM_Status status = EEPROM_ReadBuffer(bytes_copied, buffer, bytes_to_copy);
        if (status != EEPROM_OK) {
            return status;
        }

        // Write to FV1 EEPROM (I2C2)
        status = FV1_EEPROM_WriteBuffer(bytes_copied, buffer, bytes_to_copy);
        if (status != EEPROM_OK) {
            return status;
        }

        bytes_copied += bytes_to_copy;
    }

    return EEPROM_OK;
}
