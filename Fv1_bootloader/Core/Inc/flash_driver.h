#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include "stm32l0xx_hal.h"
#include "boot_config.h"

typedef enum {
    FLASH_DRV_OK = 0,
    FLASH_DRV_ERR_ADDR,
    FLASH_DRV_ERR_ERASE,
    FLASH_DRV_ERR_PROGRAM,
    FLASH_DRV_ERR_LOCK
} FlashDrv_Status;

/* Erase a single 128-byte page. Address must be page-aligned and within app region. */
FlashDrv_Status FlashDrv_ErasePage(uint32_t page_addr);

/* Erase all app flash (0x08004000 to end), including the metadata page. */
FlashDrv_Status FlashDrv_EraseAppRegion(void);

/* Program a single 32-bit word. Address must be word-aligned and within app region. */
FlashDrv_Status FlashDrv_ProgramWord(uint32_t addr, uint32_t data);

/* Program a buffer of words. len is in bytes, must be word-aligned. */
FlashDrv_Status FlashDrv_ProgramBuffer(uint32_t addr, const uint8_t *data, uint32_t len);

#endif /* FLASH_DRIVER_H */
