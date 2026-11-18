#ifndef __STM32L0xx_HAL_CONF_H
#define __STM32L0xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Module Selection -- bootloader only needs the basics */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_CRC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

/* Oscillator values */
#if !defined(HSE_VALUE)
  #define HSE_VALUE    ((uint32_t)8000000U)
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    ((uint32_t)100U)
#endif
#if !defined(MSI_VALUE)
  #define MSI_VALUE    ((uint32_t)2097000U)
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000U)
#endif
#if !defined(HSI48_VALUE)
  #define HSI48_VALUE  ((uint32_t)48000000U)
#endif
#if !defined(LSI_VALUE)
  #define LSI_VALUE    ((uint32_t)37000U)
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE    ((uint32_t)32768U)
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT  ((uint32_t)5000U)
#endif

/* System Configuration */
#define VDD_VALUE                    ((uint32_t)3300U)
#define TICK_INT_PRIORITY            ((uint32_t)3U)
#define USE_RTOS                     0U
#define PREFETCH_ENABLE              0U
#define PREREAD_ENABLE               1U
#define BUFFER_CACHE_DISABLE         0U

/* Module includes */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32l0xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32l0xx_hal_gpio.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32l0xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32l0xx_hal_flash.h"
#endif
#ifdef HAL_CRC_MODULE_ENABLED
  #include "stm32l0xx_hal_crc.h"
#endif
#ifdef HAL_PCD_MODULE_ENABLED
  #include "stm32l0xx_hal_pcd.h"
  #include "stm32l0xx_ll_usb.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32l0xx_hal_pwr.h"
#endif

#define assert_param(expr) ((void)0U)

#ifdef __cplusplus
}
#endif

#endif /* __STM32L0xx_HAL_CONF_H */
