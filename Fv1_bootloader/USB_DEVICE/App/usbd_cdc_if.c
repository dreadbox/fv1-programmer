#include "usbd_cdc_if.h"
#include "boot_config.h"
#include "flash_driver.h"
#include "crc32.h"

/* RX/TX buffers */
static uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
static uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* Multi-packet accumulation buffer */
static uint8_t cmd_buf[APP_RX_DATA_SIZE];
static volatile uint32_t rx_pos;
static volatile uint8_t pending_cmd;
static volatile uint32_t pending_len;

extern USBD_HandleTypeDef hUsbDeviceFS;
extern PCD_HandleTypeDef hpcd_USB_FS;

/* Forward declarations */
static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len);

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS = {
    CDC_Init_FS,
    CDC_DeInit_FS,
    CDC_Control_FS,
    CDC_Receive_FS
};

static void send_response(const char *msg)
{
    CDC_Transmit_FS((uint8_t *)msg, strlen(msg));
}

static uint32_t cmd_expected_len(uint8_t cmd)
{
    switch (cmd) {
    case 'P': return 133;  /* 1 + 4 addr + 128 data */
    case 'M': return 129;  /* 1 + 128 metadata */
    case 'E': case 'C': return 5;  /* 1 + 4 addr/crc */
    default:  return 1;    /* B, V, G, unknown */
    }
}

static int8_t CDC_Init_FS(void)
{
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

static int8_t CDC_DeInit_FS(void)
{
    return USBD_OK;
}

static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    return USBD_OK;
}

/*
 * Accumulate USB packets into cmd_buf. USB CDC delivers max 64 bytes
 * per callback, but commands like 'P' need 133 bytes. Once we have
 * enough data for the detected command, set pending_cmd for main loop.
 */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
    if (pending_cmd == 0) {
        uint32_t copy_len = *Len;
        if (rx_pos + copy_len > sizeof(cmd_buf))
            copy_len = sizeof(cmd_buf) - rx_pos;

        memcpy(&cmd_buf[rx_pos], Buf, copy_len);
        rx_pos += copy_len;

        if (rx_pos > 0) {
            uint32_t needed = cmd_expected_len(cmd_buf[0]);
            if (rx_pos >= needed) {
                pending_len = rx_pos;
                pending_cmd = cmd_buf[0];
            }
        }
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return USBD_OK;
}

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc == NULL)
        return USBD_FAIL;
    if (hcdc->TxState != 0)
        return USBD_BUSY;

    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

/*
 * Process pending bootloader commands in main loop context.
 *
 * Protocol:
 *   B           -> "BOOTLOADER_V1.0\n"
 *   V           -> "APP_VALID\n" / "APP_INVALID\n"
 *   E + 4-byte addr -> erase 128-byte page -> "ERASE_OK\n" / "ERASE_FAIL\n"
 *   P + 4-byte addr + 128 bytes data -> program page -> "PROG_OK\n" / "PROG_FAIL\n"
 *   M + 128-byte metadata -> write metadata -> "META_OK\n" / "META_FAIL\n"
 *   C + 4-byte CRC -> verify CRC -> "CRC_OK\n" / "CRC_MISMATCH\n"
 *   G           -> jump to app -> "JUMPING...\n"
 */
void Boot_ProcessUSBCommands(void)
{
    uint8_t cmd = pending_cmd;
    if (cmd == 0)
        return;

    switch (cmd) {

    case 'B':
        send_response("BOOTLOADER_V1.0\n");
        break;

    case 'V': {
        AppMetadata_t *meta = (AppMetadata_t *)BOOT_META_ADDR;
        if (meta->magic != APP_MAGIC) {
            send_response("APP_INVALID\n");
            break;
        }
        if (meta->app_size == 0 || meta->app_size > (BOOT_FLASH_END + 1 - BOOT_APP_ADDR)) {
            send_response("APP_INVALID\n");
            break;
        }
        uint32_t sp = *(volatile uint32_t *)BOOT_APP_ADDR;
        if (sp < 0x20000000 || sp > 0x20002000) {
            send_response("APP_INVALID\n");
            break;
        }
        uint32_t crc = CRC32_Calculate((const uint8_t *)BOOT_APP_ADDR, meta->app_size);
        if (crc != meta->crc32) {
            send_response("APP_INVALID\n");
            break;
        }
        send_response("APP_VALID\n");
        break;
    }

    case 'E': {
        if (pending_len < 5) {
            send_response("ERASE_FAIL\n");
            break;
        }
        uint32_t addr = (uint32_t)cmd_buf[1]
                       | ((uint32_t)cmd_buf[2] << 8)
                       | ((uint32_t)cmd_buf[3] << 16)
                       | ((uint32_t)cmd_buf[4] << 24);
        if (FlashDrv_ErasePage(addr) == FLASH_DRV_OK)
            send_response("ERASE_OK\n");
        else
            send_response("ERASE_FAIL\n");
        break;
    }

    case 'P': {
        if (pending_len < 133) {
            send_response("PROG_FAIL\n");
            break;
        }
        uint32_t addr = (uint32_t)cmd_buf[1]
                       | ((uint32_t)cmd_buf[2] << 8)
                       | ((uint32_t)cmd_buf[3] << 16)
                       | ((uint32_t)cmd_buf[4] << 24);
        if (FlashDrv_ProgramBuffer(addr, &cmd_buf[5], 128) == FLASH_DRV_OK)
            send_response("PROG_OK\n");
        else
            send_response("PROG_FAIL\n");
        break;
    }

    case 'M': {
        if (pending_len < 129) {
            send_response("META_FAIL\n");
            break;
        }
        if (FlashDrv_ErasePage(BOOT_META_ADDR) != FLASH_DRV_OK) {
            send_response("META_FAIL\n");
            break;
        }
        if (FlashDrv_ProgramBuffer(BOOT_META_ADDR, &cmd_buf[1], 128) == FLASH_DRV_OK)
            send_response("META_OK\n");
        else
            send_response("META_FAIL\n");
        break;
    }

    case 'C': {
        if (pending_len < 5) {
            send_response("CRC_MISMATCH\n");
            break;
        }
        uint32_t expected = (uint32_t)cmd_buf[1]
                           | ((uint32_t)cmd_buf[2] << 8)
                           | ((uint32_t)cmd_buf[3] << 16)
                           | ((uint32_t)cmd_buf[4] << 24);
        AppMetadata_t *meta = (AppMetadata_t *)BOOT_META_ADDR;
        if (meta->magic != APP_MAGIC || meta->app_size == 0) {
            send_response("CRC_MISMATCH\n");
            break;
        }
        uint32_t actual = CRC32_Calculate((const uint8_t *)BOOT_APP_ADDR, meta->app_size);
        if (actual == expected)
            send_response("CRC_OK\n");
        else
            send_response("CRC_MISMATCH\n");
        break;
    }

    case 'G': {
        send_response("JUMPING...\n");
        HAL_Delay(50);  /* let USB response flush */

        uint32_t app_reset = *(volatile uint32_t *)(BOOT_APP_ADDR + 4);
        if (app_reset < 0x08000000 || app_reset > BOOT_FLASH_END) {
            break;
        }

        uint32_t app_msp = *(volatile uint32_t *)BOOT_APP_ADDR;
        HAL_PCD_DeInit(&hpcd_USB_FS);
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL  = 0;
        SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;
        __set_MSP(app_msp);
        ((void (*)(void))app_reset)();
        break;
    }

    default:
        send_response("UNKNOWN\n");
        break;
    }

    pending_cmd = 0;
    rx_pos = 0;
}
