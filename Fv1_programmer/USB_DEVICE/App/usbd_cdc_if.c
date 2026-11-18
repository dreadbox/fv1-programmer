/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @version        : v2.0_Cube
  * @brief          : Usb device for Virtual Com Port.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include "eeprom_24lc32.h"
#include "gpio.h"
#include <string.h>

extern void RequestBootloader(void);
extern EEPROM_Status CopyToFV1(void);

/* uint32 -> decimal ASCII (no snprintf to avoid newlib-nano stdio pulling
 * in a _reent struct that overflows the 8 KB SRAM budget). Returns the
 * number of bytes written; never writes a NUL. */
static uint8_t u32_to_dec(uint32_t value, char *out)
{
  if (value == 0) {
    out[0] = '0';
    return 1;
  }
  char tmp[10];
  uint8_t n = 0;
  while (value) {
    tmp[n++] = (char)('0' + (value % 10u));
    value /= 10u;
  }
  for (uint8_t i = 0; i < n; i++) {
    out[i] = tmp[n - 1 - i];
  }
  return n;
}
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions USBD_CDC_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

// Protocol state machine
typedef enum {
    PROTO_IDLE,          // Waiting for command
    PROTO_READING,       // Reading EEPROM (sending to host)
    PROTO_WRITING,       // Writing EEPROM (receiving from host)
} ProtocolState;

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines USBD_CDC_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

// Protocol commands
#define CMD_READ        'R'
#define CMD_WRITE       'W'
#define CMD_TEST        'T'  // Test command - echo back "OK"
#define CMD_INFO        'I'  // Info command - return device identifier
#define CMD_VERSION     '?'  // Returns running firmware version from bootloader metadata
#define CMD_COPY        'C'  // Copy local EEPROM to FV-1 target (same as button press)

/* App metadata written by the bootloader at 0x08003F80:
 *   { uint32_t magic; uint32_t app_size; uint32_t crc32; uint32_t version; ... } */
#define APP_METADATA_ADDR   0x08003F80U
#define APP_METADATA_MAGIC  0xDEADBEEFU

// EEPROM size
#define EEPROM_SIZE     4096

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros USBD_CDC_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables USBD_CDC_IF_Private_Variables
  * @brief Private variables.
  * @{
  */
/* Create buffer for reception and transmission           */
/* It's up to user to redefine and/or remove those define */
/** Received data over USB are stored in this buffer      */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];

/** Data to send over USB CDC are stored in this buffer   */
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */

// Protocol state
static ProtocolState proto_state = PROTO_IDLE;
static WriteStatus write_status = WRITE_STATUS_IDLE;
static uint8_t pending_command = 0;  // Command to process in main loop

// DTR line state from host -- set when an app opens the COM port
static volatile uint8_t dtr_asserted = 0;

// Write buffer - 1KB chunk buffer (write to EEPROM in 4 chunks to save RAM)
#define WRITE_CHUNK_SIZE 1024
static uint8_t write_buffer[WRITE_CHUNK_SIZE];
static uint32_t write_buffer_offset = 0;
static uint32_t total_bytes_written = 0;  // Track total bytes written across all chunks

// Line coding structure for CDC (type already defined in usbd_cdc.h)
static USBD_CDC_LineCodingTypeDef LineCoding = {
  115200, /* baud rate */
  0x00,   /* stop bits-1 */
  0x00,   /* parity - none */
  0x08    /* nb. of bits 8 */
};

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables USBD_CDC_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes USBD_CDC_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t* pbuf, uint32_t *Len);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the CDC media low layer over the FS USB IP
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  /* Set Application Buffers */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);  // Start receiving data
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  DeInitializes the CDC media low layer
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  dtr_asserted = 0;  // host disconnect -- forget any prior DTR state
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage the CDC class requests
  * @param  cmd: Command code
  * @param  pbuf: Buffer containing command data (request parameters)
  * @param  length: Number of data to be sent (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  switch(cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:

    break;

    case CDC_GET_ENCAPSULATED_RESPONSE:

    break;

    case CDC_SET_COMM_FEATURE:

    break;

    case CDC_GET_COMM_FEATURE:

    break;

    case CDC_CLEAR_COMM_FEATURE:

    break;

  /*******************************************************************************/
  /* Line Coding Structure                                                       */
  /*-----------------------------------------------------------------------------*/
  /* Offset | Field       | Size | Value  | Description                          */
  /* 0      | dwDTERate   |   4  | Number |Data terminal rate, in bits per second*/
  /* 4      | bCharFormat |   1  | Number | Stop bits                            */
  /*                                        0 - 1 Stop bit                       */
  /*                                        1 - 1.5 Stop bits                    */
  /*                                        2 - 2 Stop bits                      */
  /* 5      | bParityType |  1   | Number | Parity                               */
  /*                                        0 - None                             */
  /*                                        1 - Odd                              */
  /*                                        2 - Even                             */
  /*                                        3 - Mark                             */
  /*                                        4 - Space                            */
  /* 6      | bDataBits  |   1   | Number Data bits (5, 6, 7, 8 or 16).          */
  /*******************************************************************************/
    case CDC_SET_LINE_CODING:
      LineCoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
      LineCoding.format     = pbuf[4];
      LineCoding.paritytype = pbuf[5];
      LineCoding.datatype   = pbuf[6];
    break;

    case CDC_GET_LINE_CODING:
      pbuf[0] = (uint8_t)(LineCoding.bitrate);
      pbuf[1] = (uint8_t)(LineCoding.bitrate >> 8);
      pbuf[2] = (uint8_t)(LineCoding.bitrate >> 16);
      pbuf[3] = (uint8_t)(LineCoding.bitrate >> 24);
      pbuf[4] = LineCoding.format;
      pbuf[5] = LineCoding.paritytype;
      pbuf[6] = LineCoding.datatype;
    break;

    case CDC_SET_CONTROL_LINE_STATE:
      /* SET_CONTROL_LINE_STATE has no OUT data stage, so the CDC core
       * passes the raw setup packet via pbuf. wValue bit 0 = DTR. */
      if (pbuf != NULL) {
        USBD_SetupReqTypedef *req = (USBD_SetupReqTypedef *)pbuf;
        dtr_asserted = (req->wValue & 0x01u) ? 1u : 0u;
      }
    break;

    case CDC_SEND_BREAK:

    break;

  default:
    break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface
  *         through this function.
  *
  *         @note
  *         This function will issue a NAK packet on any OUT packet received on
  *         USB endpoint until exiting this function. If you exit this function
  *         before transfer is complete on CDC interface (ie. using DMA controller)
  *         it will result in receiving more data while previous ones are still
  *         not sent.
  *
  * @param  Buf: Buffer of data to be received
  * @param  Len: Number of data received (in bytes)
  * @retval Result of the operation: USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t CDC_Receive_FS(uint8_t* Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */

  uint32_t len = *Len;

  switch (proto_state) {
    case PROTO_IDLE:
      // Waiting for command
      if (len >= 1) {
        if (Buf[0] == CMD_TEST) {
          // Test command - immediately respond
          const char *test_msg = "TEST_OK\n";
          CDC_Transmit_FS((uint8_t*)test_msg, strlen(test_msg));
        } else if (Buf[0] == CMD_INFO) {
          // Info command - return device identifier
          const char *info_msg = "FV1_PROGRAMMER_V1.0\n";
          CDC_Transmit_FS((uint8_t*)info_msg, strlen(info_msg));
        } else if (Buf[0] == 'U') {
          pending_command = 'U';
        } else if (Buf[0] == CMD_VERSION) {
          pending_command = CMD_VERSION;
        } else if (Buf[0] == CMD_COPY) {
          pending_command = CMD_COPY;
        } else if (Buf[0] == CMD_READ) {
          // Set flag for main loop to process READ command immediately
          pending_command = CMD_READ;
          proto_state = PROTO_READING;
        } else if (Buf[0] == CMD_WRITE) {
          // WRITE command - expect 4096 bytes to follow (in 1KB chunks)
          proto_state = PROTO_WRITING;
          write_buffer_offset = 0;
          total_bytes_written = 0;
          write_status = WRITE_STATUS_IDLE;
        }
      }
      break;

    case PROTO_WRITING:
      // Buffer incoming data - only copy what fits in current chunk
      if (total_bytes_written + write_buffer_offset + len <= EEPROM_SIZE) {
        // Calculate how much we can copy without overflowing the 1KB buffer
        uint32_t space_in_buffer = WRITE_CHUNK_SIZE - write_buffer_offset;
        uint32_t bytes_to_copy = (len <= space_in_buffer) ? len : space_in_buffer;

        // Copy to buffer
        memcpy(&write_buffer[write_buffer_offset], Buf, bytes_to_copy);
        write_buffer_offset += bytes_to_copy;

        // Check if chunk is ready to write
        bool chunk_full = (write_buffer_offset >= WRITE_CHUNK_SIZE);
        bool final_data = (total_bytes_written + write_buffer_offset >= EEPROM_SIZE);

        if (chunk_full || final_data) {
          // Flag for main loop to write this chunk to EEPROM
          pending_command = CMD_WRITE;
          write_status = WRITE_STATUS_IN_PROGRESS;
        }
      } else {
        // Too much data received
        write_status = WRITE_STATUS_ERROR;
        proto_state = PROTO_IDLE;
        write_buffer_offset = 0;
        total_bytes_written = 0;

        const char *error_msg = "ERROR\n";
        CDC_Transmit_FS((uint8_t*)error_msg, strlen(error_msg));
      }
      break;

    default:
      proto_state = PROTO_IDLE;
      break;
  }

  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &Buf[0]);

  // Only re-enable receive if we're not waiting for a pending write
  // This prevents data loss when buffer is full
  if (pending_command != CMD_WRITE) {
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  }

  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  CDC_Transmit_FS
  *         Data to send over USB IN endpoint are sent over CDC interface
  *         through this function.
  *         @note
  *
  *
  * @param  Buf: Buffer of data to be sent
  * @param  Len: Number of data to be sent (in bytes)
  * @retval USBD_OK if all operations are OK else USBD_FAIL or USBD_BUSY
  */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
  if (hcdc->TxState != 0){
    return USBD_BUSY;
  }
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, Buf, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @brief  Get current write status
  * @retval Write status
  */
WriteStatus CDC_GetWriteStatus(void)
{
  return write_status;
}

/**
  * @brief  Reset write status (called after LED indication)
  * @retval None
  */
void CDC_ResetWriteStatus(void)
{
  write_status = WRITE_STATUS_IDLE;
}

/**
  * @brief  Returns non-zero when a host application has opened the COM port
  *         (DTR asserted via CDC SET_CONTROL_LINE_STATE). Cleared on USB
  *         disconnect via CDC_DeInit_FS.
  */
uint8_t CDC_IsPortOpen(void)
{
  return dtr_asserted;
}

/**
  * @brief  Process pending READ/WRITE commands (call from main loop)
  * @retval None
  */
void CDC_ProcessPendingCommand(void)
{
  if (pending_command == CMD_READ && proto_state == PROTO_READING) {
    pending_command = 0;

    // Read EEPROM and send in chunks
    for (uint32_t offset = 0; offset < EEPROM_SIZE; offset += APP_TX_DATA_SIZE) {
      uint16_t chunk_size = (EEPROM_SIZE - offset) > APP_TX_DATA_SIZE ?
                             APP_TX_DATA_SIZE : (EEPROM_SIZE - offset);

      EEPROM_Status status = EEPROM_ReadBuffer(offset, UserTxBufferFS, chunk_size);

      if (status == EEPROM_OK) {
        // Wait until USB is ready and transmit
        uint8_t tx_result = USBD_BUSY;
        uint32_t timeout = 10000;

        while (tx_result != USBD_OK && timeout > 0) {
          tx_result = CDC_Transmit_FS(UserTxBufferFS, chunk_size);
          if (tx_result == USBD_BUSY) {
            // Small delay before retry
            for (volatile uint32_t i = 0; i < 1000; i++);
          }
          timeout--;
        }

        if (tx_result != USBD_OK) {
          break;  // Transmission failed, abort
        }

        // Wait for transmission to complete before next chunk
        timeout = 10000;
        while (timeout > 0) {
          USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
          if (hcdc->TxState == 0) break;
          for (volatile uint32_t i = 0; i < 100; i++);
          timeout--;
        }
      } else {
        break;  // EEPROM read failed, abort
      }
    }

    proto_state = PROTO_IDLE;
  }
  else if (pending_command == CMD_WRITE && proto_state == PROTO_WRITING) {
    pending_command = 0;

    // Write current chunk to EEPROM
    uint16_t chunk_size = write_buffer_offset;
    EEPROM_Status status = EEPROM_WriteBuffer(total_bytes_written, write_buffer, chunk_size);

    if (status == EEPROM_OK) {
      // Update totals
      total_bytes_written += chunk_size;
      write_buffer_offset = 0;

      // Re-enable USB receive now that buffer is cleared
      USBD_CDC_ReceivePacket(&hUsbDeviceFS);

      // Check if all chunks written
      if (total_bytes_written >= EEPROM_SIZE) {
        // All done!
        write_status = WRITE_STATUS_SUCCESS;
        proto_state = PROTO_IDLE;
        total_bytes_written = 0;

        // Send success response
        const char *success_msg = "OK\n";
        CDC_Transmit_FS((uint8_t*)success_msg, strlen(success_msg));
      }
      // else: waiting for more chunks, stay in PROTO_WRITING state
    } else {
      // Write error
      write_status = WRITE_STATUS_ERROR;
      proto_state = PROTO_IDLE;
      write_buffer_offset = 0;
      total_bytes_written = 0;

      // Re-enable USB receive
      USBD_CDC_ReceivePacket(&hUsbDeviceFS);

      // Send error response
      const char *error_msg = "ERROR\n";
      CDC_Transmit_FS((uint8_t*)error_msg, strlen(error_msg));
    }
  }
  else if (pending_command == 'U') {
    pending_command = 0;
    const char *msg = "REBOOTING...\n";
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    HAL_Delay(50);
    RequestBootloader();
  }
  else if (pending_command == CMD_COPY) {
    pending_command = 0;
    EEPROM_Status status = CopyToFV1();
    const char *msg = (status == EEPROM_OK) ? "COPY_OK\n" : "COPY_FAIL\n";
    CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
  }
  else if (pending_command == CMD_VERSION) {
    pending_command = 0;
    const volatile uint32_t *meta = (const volatile uint32_t *)APP_METADATA_ADDR;
    if (meta[0] != APP_METADATA_MAGIC) {
      const char *msg = "META_INVALID\n";
      CDC_Transmit_FS((uint8_t*)msg, strlen(msg));
    } else {
      char buf[24];
      const char prefix[] = "VERSION ";
      memcpy(buf, prefix, sizeof(prefix) - 1);
      uint8_t n = sizeof(prefix) - 1;
      n += u32_to_dec(meta[3], &buf[n]);
      buf[n++] = '\n';
      CDC_Transmit_FS((uint8_t*)buf, n);
    }
  }
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
