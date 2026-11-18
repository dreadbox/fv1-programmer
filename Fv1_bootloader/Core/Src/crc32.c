#include "crc32.h"
#include "stm32l0xx_hal.h"

static CRC_HandleTypeDef hcrc;

void CRC32_Init(void)
{
    __HAL_RCC_CRC_CLK_ENABLE();

    hcrc.Instance = CRC;
    hcrc.Init.DefaultPolynomialUse    = DEFAULT_POLYNOMIAL_ENABLE;
    hcrc.Init.DefaultInitValueUse     = DEFAULT_INIT_VALUE_ENABLE;
    hcrc.Init.InputDataInversionMode  = CRC_INPUTDATA_INVERSION_BYTE;
    hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
    hcrc.InputDataFormat              = CRC_INPUTDATA_FORMAT_BYTES;
    HAL_CRC_Init(&hcrc);
}

uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len)
{
    uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)data, len);
    return crc ^ 0xFFFFFFFF;
}
