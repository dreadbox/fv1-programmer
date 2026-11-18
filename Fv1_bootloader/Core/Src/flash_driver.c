#include "flash_driver.h"

#define FLASH_PAGE_SZ       128U
#define APP_REGION_START    BOOT_META_ADDR  /* metadata page is erasable too */
#define APP_REGION_END      (BOOT_FLASH_END + 1U)

static int addr_in_app_region(uint32_t addr)
{
    return (addr >= APP_REGION_START && addr <= BOOT_FLASH_END);
}

FlashDrv_Status FlashDrv_ErasePage(uint32_t page_addr)
{
    if ((page_addr & (FLASH_PAGE_SZ - 1)) != 0)
        return FLASH_DRV_ERR_ADDR;

    if (!addr_in_app_region(page_addr))
        return FLASH_DRV_ERR_ADDR;

    FLASH_EraseInitTypeDef erase;
    uint32_t page_error;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = page_addr;
    erase.NbPages     = 1;

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef rc = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    return (rc == HAL_OK) ? FLASH_DRV_OK : FLASH_DRV_ERR_ERASE;
}

FlashDrv_Status FlashDrv_EraseAppRegion(void)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error;
    uint32_t nb_pages = (APP_REGION_END - APP_REGION_START) / FLASH_PAGE_SZ;

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = APP_REGION_START;
    erase.NbPages     = nb_pages;

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef rc = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    return (rc == HAL_OK) ? FLASH_DRV_OK : FLASH_DRV_ERR_ERASE;
}

FlashDrv_Status FlashDrv_ProgramWord(uint32_t addr, uint32_t data)
{
    if ((addr & 3U) != 0)
        return FLASH_DRV_ERR_ADDR;

    if (!addr_in_app_region(addr))
        return FLASH_DRV_ERR_ADDR;

    HAL_FLASH_Unlock();
    HAL_StatusTypeDef rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, data);
    HAL_FLASH_Lock();

    return (rc == HAL_OK) ? FLASH_DRV_OK : FLASH_DRV_ERR_PROGRAM;
}

FlashDrv_Status FlashDrv_ProgramBuffer(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if ((addr & 3U) != 0 || (len & 3U) != 0)
        return FLASH_DRV_ERR_ADDR;

    if (!addr_in_app_region(addr) || !addr_in_app_region(addr + len - 1))
        return FLASH_DRV_ERR_ADDR;

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = (uint32_t)data[i]
                      | ((uint32_t)data[i + 1] << 8)
                      | ((uint32_t)data[i + 2] << 16)
                      | ((uint32_t)data[i + 3] << 24);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
            HAL_FLASH_Lock();
            return FLASH_DRV_ERR_PROGRAM;
        }
    }

    HAL_FLASH_Lock();
    return FLASH_DRV_OK;
}
