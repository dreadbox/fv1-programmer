#ifndef __USBD_CONF__H__
#define __USBD_CONF__H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "stm32l0xx.h"
#include "stm32l0xx_hal.h"

#define USBD_MAX_NUM_INTERFACES     1U
#define USBD_MAX_NUM_CONFIGURATION  1U
#define USBD_MAX_STR_DESC_SIZ       512U
#define USBD_DEBUG_LEVEL            0U
#define USBD_SELF_POWERED           1U

#define DEVICE_FS 0

#define USBD_malloc         (uint32_t *)USBD_static_malloc
#define USBD_free           USBD_static_free
#define USBD_memset
#define USBD_memcpy
#define USBD_Delay          HAL_Delay

#if (USBD_DEBUG_LEVEL > 0)
#define USBD_UsrLog(...)    printf(__VA_ARGS__); printf("\n");
#else
#define USBD_UsrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 1)
#define USBD_ErrLog(...)    printf("ERROR: "); printf(__VA_ARGS__); printf("\n");
#else
#define USBD_ErrLog(...)
#endif

#if (USBD_DEBUG_LEVEL > 2)
#define USBD_DbgLog(...)    printf("DEBUG : "); printf(__VA_ARGS__); printf("\n");
#else
#define USBD_DbgLog(...)
#endif

void *USBD_static_malloc(uint32_t size);
void USBD_static_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF__H__ */
