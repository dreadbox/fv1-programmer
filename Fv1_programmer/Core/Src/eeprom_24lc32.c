/**
  ******************************************************************************
  * @file           : eeprom_24lc32.c
  * @brief          : Driver implementation for 24LC32AT I2C EEPROM
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "eeprom_24lc32.h"

/* External Variables --------------------------------------------------------*/
extern I2C_HandleTypeDef hi2c1;

/* Private Functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the EEPROM driver
  */
EEPROM_Status EEPROM_Init(void)
{
    // Check if EEPROM is present by attempting to communicate
    if (HAL_I2C_IsDeviceReady(&EEPROM_I2C_HANDLE,
                              EEPROM_I2C_ADDRESS << 1,
                              3,
                              100) == HAL_OK)
    {
        return EEPROM_OK;
    }
    return EEPROM_ERROR;
}

/**
  * @brief  Read a single byte from EEPROM
  */
EEPROM_Status EEPROM_ReadByte(uint16_t addr, uint8_t *data)
{
    if (addr >= EEPROM_SIZE || data == NULL) {
        return EEPROM_INVALID_PARAM;
    }

    // Set memory address (2 bytes for 24LC32)
    uint8_t addr_bytes[2];
    addr_bytes[0] = (addr >> 8) & 0xFF;  // High byte
    addr_bytes[1] = addr & 0xFF;         // Low byte

    // Write address then read data
    if (HAL_I2C_Master_Transmit(&EEPROM_I2C_HANDLE,
                                EEPROM_I2C_ADDRESS << 1,
                                addr_bytes,
                                2,
                                HAL_MAX_DELAY) != HAL_OK)
    {
        return EEPROM_ERROR;
    }

    if (HAL_I2C_Master_Receive(&EEPROM_I2C_HANDLE,
                               EEPROM_I2C_ADDRESS << 1,
                               data,
                               1,
                               HAL_MAX_DELAY) != HAL_OK)
    {
        return EEPROM_ERROR;
    }

    return EEPROM_OK;
}

/**
  * @brief  Write a single byte to EEPROM
  */
EEPROM_Status EEPROM_WriteByte(uint16_t addr, uint8_t data)
{
    if (addr >= EEPROM_SIZE) {
        return EEPROM_INVALID_PARAM;
    }

    // Prepare data: [addr_high, addr_low, data]
    uint8_t buffer[3];
    buffer[0] = (addr >> 8) & 0xFF;  // High byte
    buffer[1] = addr & 0xFF;         // Low byte
    buffer[2] = data;

    // Write address and data
    if (HAL_I2C_Master_Transmit(&EEPROM_I2C_HANDLE,
                                EEPROM_I2C_ADDRESS << 1,
                                buffer,
                                3,
                                HAL_MAX_DELAY) != HAL_OK)
    {
        return EEPROM_ERROR;
    }

    // Wait for write cycle to complete (ACK polling)
    return EEPROM_WaitReady(EEPROM_WRITE_TIMEOUT);
}

/**
  * @brief  Read multiple bytes from EEPROM
  */
EEPROM_Status EEPROM_ReadBuffer(uint16_t addr, uint8_t *buffer, uint16_t size)
{
    if (addr >= EEPROM_SIZE || buffer == NULL || size == 0) {
        return EEPROM_INVALID_PARAM;
    }

    // Check if read would exceed EEPROM size
    if ((uint32_t)addr + size > EEPROM_SIZE) {
        return EEPROM_INVALID_PARAM;
    }

    // Set memory address
    uint8_t addr_bytes[2];
    addr_bytes[0] = (addr >> 8) & 0xFF;
    addr_bytes[1] = addr & 0xFF;

    // Write address
    if (HAL_I2C_Master_Transmit(&EEPROM_I2C_HANDLE,
                                EEPROM_I2C_ADDRESS << 1,
                                addr_bytes,
                                2,
                                HAL_MAX_DELAY) != HAL_OK)
    {
        return EEPROM_ERROR;
    }

    // Read data (sequential read)
    if (HAL_I2C_Master_Receive(&EEPROM_I2C_HANDLE,
                               EEPROM_I2C_ADDRESS << 1,
                               buffer,
                               size,
                               HAL_MAX_DELAY) != HAL_OK)
    {
        return EEPROM_ERROR;
    }

    return EEPROM_OK;
}

/**
  * @brief  Write multiple bytes to EEPROM (handles page boundaries)
  */
EEPROM_Status EEPROM_WriteBuffer(uint16_t addr, uint8_t *buffer, uint16_t size)
{
    if (addr >= EEPROM_SIZE || buffer == NULL || size == 0) {
        return EEPROM_INVALID_PARAM;
    }

    // Check if write would exceed EEPROM size
    if ((uint32_t)addr + size > EEPROM_SIZE) {
        return EEPROM_INVALID_PARAM;
    }

    uint16_t bytes_written = 0;

    while (bytes_written < size) {
        // Calculate current page and remaining space in page
        uint16_t current_addr = addr + bytes_written;
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t bytes_to_page_end = EEPROM_PAGE_SIZE - page_offset;

        // Calculate how many bytes to write in this iteration
        uint16_t bytes_to_write = (size - bytes_written) < bytes_to_page_end ?
                                  (size - bytes_written) : bytes_to_page_end;

        // Prepare write buffer: [addr_high, addr_low, data...]
        uint8_t write_buffer[EEPROM_PAGE_SIZE + 2];
        write_buffer[0] = (current_addr >> 8) & 0xFF;
        write_buffer[1] = current_addr & 0xFF;

        // Copy data to write buffer
        for (uint16_t i = 0; i < bytes_to_write; i++) {
            write_buffer[2 + i] = buffer[bytes_written + i];
        }

        // Write page
        if (HAL_I2C_Master_Transmit(&EEPROM_I2C_HANDLE,
                                    EEPROM_I2C_ADDRESS << 1,
                                    write_buffer,
                                    bytes_to_write + 2,
                                    HAL_MAX_DELAY) != HAL_OK)
        {
            return EEPROM_ERROR;
        }

        // Wait for write cycle to complete
        if (EEPROM_WaitReady(EEPROM_WRITE_TIMEOUT) != EEPROM_OK) {
            return EEPROM_TIMEOUT;
        }

        bytes_written += bytes_to_write;
    }

    return EEPROM_OK;
}

/**
  * @brief  Erase entire EEPROM (fill with 0xFF)
  */
EEPROM_Status EEPROM_EraseFull(void)
{
    uint8_t erase_buffer[EEPROM_PAGE_SIZE];

    // Fill buffer with 0xFF
    for (uint16_t i = 0; i < EEPROM_PAGE_SIZE; i++) {
        erase_buffer[i] = 0xFF;
    }

    // Erase all pages
    for (uint16_t page = 0; page < EEPROM_SIZE / EEPROM_PAGE_SIZE; page++) {
        uint16_t addr = page * EEPROM_PAGE_SIZE;

        if (EEPROM_WriteBuffer(addr, erase_buffer, EEPROM_PAGE_SIZE) != EEPROM_OK) {
            return EEPROM_ERROR;
        }
    }

    return EEPROM_OK;
}

/**
  * @brief  Check if EEPROM is ready (ACK polling)
  */
bool EEPROM_IsReady(void)
{
    return (HAL_I2C_IsDeviceReady(&EEPROM_I2C_HANDLE,
                                  EEPROM_I2C_ADDRESS << 1,
                                  1,
                                  1) == HAL_OK);
}

/**
  * @brief  Wait for EEPROM write cycle to complete
  * @note   Modified to be non-blocking for USB operations - no HAL_Delay!
  */
EEPROM_Status EEPROM_WaitReady(uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms) {
        if (EEPROM_IsReady()) {
            return EEPROM_OK;
        }
        // NO HAL_Delay here! Just poll continuously for USB compatibility
        // This uses more CPU but prevents USB stack blocking
    }

    return EEPROM_TIMEOUT;
}
