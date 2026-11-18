#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

#include <stdint.h>

/* Flash memory map */
#define BOOT_APP_ADDR       0x08004000  /* Application start (16K offset) */
#define BOOT_META_ADDR      0x08003F80  /* Metadata block (128 bytes before app) */
#define BOOT_FLASH_END      0x0800FFFF  /* End of 64K flash */

/* Magic numbers */
#define APP_MAGIC           0xDEADBEEF
#define BOOT_REQUEST_MAGIC  0xB00710AD

/* Boot request word in RAM -- survives software reset.
 * Must match the address used in the app.
 * Placed well below stack top (0x20002000) to survive startup stack usage. */
#define BOOT_REQUEST_ADDR   ((volatile uint32_t *)0x20001F00)

/* Boot button (PB11, active LOW) */
#define BOOT_BUTTON_PIN     GPIO_PIN_11
#define BOOT_BUTTON_PORT    GPIOB

/* Application metadata stored at BOOT_META_ADDR */
typedef struct {
    uint32_t magic;         /* Must be APP_MAGIC */
    uint32_t app_size;      /* Size in bytes */
    uint32_t crc32;         /* CRC32 of application image */
    uint32_t version;       /* Firmware version (major.minor.patch packed) */
    uint8_t  reserved[112]; /* Pad to 128 bytes */
} AppMetadata_t;

#endif /* BOOT_CONFIG_H */
