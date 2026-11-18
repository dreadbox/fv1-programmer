/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gpio.h"
#include "eeprom_24lc32.h"
#include "eeprom_fv1.h"
#include "usbd_cdc_if.h"
#include <stdbool.h>
#include <string.h>

/* Bootloader request: write magic word to fixed RAM address, then reset */
#define BOOT_REQUEST_MAGIC  0xB00710AD
#define BOOT_REQUEST_ADDR   ((volatile uint32_t *)0x20001F00)

void RequestBootloader(void)
{
    *BOOT_REQUEST_ADDR = BOOT_REQUEST_MAGIC;
    NVIC_SystemReset();
}

/*
 * Cortex-M0+ vector table relocation via SYSCFG memory remap.
 * M0+ has no VTOR register. To relocate vectors when running from 0x08004000,
 * we copy the vector table to SRAM base and remap SRAM to address 0x00000000.
 * Must be called before HAL_Init() (which enables SysTick).
 */
#define APP_VECTOR_TABLE_ADDR  0x08004000
#define VECTOR_TABLE_SIZE      48  /* 16 system + 32 IRQ on STM32L053 */

static void RelocateVectorTable(void)
{
    volatile uint32_t *src = (volatile uint32_t *)APP_VECTOR_TABLE_ADDR;
    volatile uint32_t *dst = (volatile uint32_t *)0x20000000;

    for (int i = 0; i < VECTOR_TABLE_SIZE; i++)
        dst[i] = src[i];

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_SYSCFG_REMAPMEMORY_SRAM();
}

/* App version stamp -- placed by the linker into the .app_version section
 * immediately after the vector table (at 0x080040C0). The host updater
 * reads {magic, version} from the .bin file before flashing and writes
 * `version` into the AppMetadata block. Bump this manually on release. */
__attribute__((section(".app_version"), used))
const struct { uint32_t magic; uint32_t version; } __app_version_stamp = {
    0x53524556U,  /* 'V','E','R','S' little-endian */
    1U
};
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

// Application state machine
typedef enum {
    STATE_IDLE,          // No write in progress, slow blink
    STATE_PENDING,       // Writing to EEPROM, fast blink
    STATE_COPYING,       // Copying from I2C1 to I2C2, rapid blink
    STATE_SUCCESS,       // Write successful, solid ON for 2s
    STATE_ERROR          // Write failed, triple blink pattern
} AppState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Button configuration (PB11)
#define BUTTON_PIN       GPIO_PIN_11
#define BUTTON_GPIO_PORT GPIOB
#define BUTTON_DEBOUNCE_MS  50

#define ERROR_DISPLAY_MS    3000

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

// Application state
static volatile AppState app_state = STATE_IDLE;
static uint32_t state_timer = 0;

// Button state for debouncing
static uint32_t button_last_press = 0;
static bool button_was_pressed = false;

// Track if current operation is button-triggered (not USB)
static bool button_operation = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Function prototypes
static void UpdateLED(void);
static void UpdateStateFromSerial(void);

/* hUsbDeviceFS is defined in USB_DEVICE/App/usb_device.c */
extern USBD_HandleTypeDef hUsbDeviceFS;

// LED Update: LED1 = host app has the COM port open (DTR), LED2 = activity / error
void UpdateLED(void)
{
    /* LED1: a host application has opened the virtual COM port (DTR asserted).
     * Gated on USBD_STATE_CONFIGURED so unplug clears LED1 even if the host
     * never explicitly dropped DTR. */
    if (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED && CDC_IsPortOpen()) {
        LED1_On();
    } else {
        LED1_Off();
    }

    /* LED2: static states only -- PENDING/COPYING blink is driven from
     * SysTick (LEDActivity_Tick) so it keeps blinking through blocking I/O. */
    switch (app_state) {
        case STATE_PENDING:
        case STATE_COPYING:
            /* handled by LEDActivity_Tick() in SysTick */
            break;

        case STATE_ERROR:
            /* Solid ON for ERROR_DISPLAY_MS, then back to idle. The CDC write
             * status must be cleared too, or UpdateStateFromSerial() would
             * immediately re-enter ERROR after a failed USB write. */
            if (HAL_GetTick() - state_timer >= ERROR_DISPLAY_MS) {
                CDC_ResetWriteStatus();
                app_state = STATE_IDLE;
                LED2_Off();
            } else {
                LED2_On();
            }
            break;

        case STATE_SUCCESS:
            /* No visual celebration -- silently return to idle */
            if (!button_operation) {
                CDC_ResetWriteStatus();
            }
            button_operation = false;
            app_state = STATE_IDLE;
            LED2_Off();
            break;

        case STATE_IDLE:
        default:
            LED2_Off();
            break;
    }
}

/* Called from SysTick (1ms) -- toggles LED2 every 100ms during write/copy
 * activity so the blink stays visible even when the main loop is parked
 * inside a blocking I2C transfer. */
void LEDActivity_Tick(void)
{
    static uint32_t last_toggle = 0;

    if (app_state == STATE_PENDING || app_state == STATE_COPYING) {
        uint32_t tick = HAL_GetTick();
        if (tick - last_toggle >= 100) {
            LED2_Toggle();
            last_toggle = tick;
        }
    }
}

// Update application state based on serial write status
void UpdateStateFromSerial(void)
{
    // Don't interfere with button operations
    if (button_operation) {
        return;
    }

    WriteStatus status = CDC_GetWriteStatus();

    switch (status) {
        case WRITE_STATUS_IDLE:
            if (app_state != STATE_SUCCESS && app_state != STATE_ERROR) {
                app_state = STATE_IDLE;
            }
            break;

        case WRITE_STATUS_IN_PROGRESS:
            // Write in progress
            app_state = STATE_PENDING;
            break;

        case WRITE_STATUS_SUCCESS:
            // Write completed successfully
            if (app_state != STATE_SUCCESS) {
                app_state = STATE_SUCCESS;
                state_timer = HAL_GetTick();
            }
            break;

        case WRITE_STATUS_ERROR:
            // Write error
            if (app_state != STATE_ERROR) {
                app_state = STATE_ERROR;
                state_timer = HAL_GetTick();
            }
            break;
    }
}

// Copy EEPROM data from I2C1 to I2C2 (to FV1). Shared by the button path and
// the CDC 'C' command -- both want the same LED behavior and need to know if
// the copy succeeded.
EEPROM_Status CopyToFV1(void)
{
    // Guard UpdateStateFromSerial from clobbering the state we set below
    button_operation = true;

    // Enter COPYING state -- SysTick LEDActivity_Tick() will blink LED2
    // throughout the blocking copy below.
    app_state = STATE_COPYING;

    // Perform the copy operation (this blocks)
    EEPROM_Status status = FV1_EEPROM_CopyFromSource();

    // Update state based on result
    if (status == EEPROM_OK) {
        app_state = STATE_SUCCESS;
    } else {
        app_state = STATE_ERROR;
        state_timer = HAL_GetTick();
    }

    // Release the UpdateStateFromSerial guard so subsequent USB ops aren't gated
    button_operation = false;

    return status;
}

// Check button and trigger copy on press
void CheckButton(void)
{
    uint32_t tick = HAL_GetTick();

    // Read button state (active LOW - button pressed when pin is LOW)
    GPIO_PinState button_state = HAL_GPIO_ReadPin(BUTTON_GPIO_PORT, BUTTON_PIN);
    bool button_pressed = (button_state == GPIO_PIN_RESET);

    // Detect button press (transition from not pressed to pressed)
    if (button_pressed && !button_was_pressed) {
        // Debounce check
        if ((tick - button_last_press) > BUTTON_DEBOUNCE_MS) {
            // Allow retry while the error is still showing
            if (app_state == STATE_IDLE || app_state == STATE_ERROR) {
                button_last_press = tick;
                CopyToFV1();  // Trigger copy operation
            }
        }
    }

    button_was_pressed = button_pressed;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  RelocateVectorTable();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  // MX_TIM2_Init();  // Disabled - TIM2 not used, conflicts with LED GPIO (PA0/PA1)
  // MX_FATFS_Init();  // Disabled - FatFs not used with CDC serial interface
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  // USB CDC Serial Protocol Architecture
  // Send 'R' to read EEPROM, send 'W' + 4KB data to write

  // Initialize EEPROM
  EEPROM_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    // Process any pending CDC commands (READ)
    CDC_ProcessPendingCommand();

    // Update state based on serial write status
    UpdateStateFromSerial();

    // Check button for copy trigger
    CheckButton();

    // Update LED based on state
    UpdateLED();

    // Small delay to prevent tight loop
    HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_4;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_USB;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00B07CB4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00B07CB4;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pins : LED1 (PA0) and LED2 (PA1) as outputs */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB11 (Button - active LOW) */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;  // Pull-up for active LOW button
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
