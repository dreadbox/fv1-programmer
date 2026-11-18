#include "main.h"
#include "boot_config.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "crc32.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

static int should_stay_in_bootloader(void)
{
    uint32_t req = *BOOT_REQUEST_ADDR;
    *BOOT_REQUEST_ADDR = 0;

    if (req == BOOT_REQUEST_MAGIC)
        return 1;

    if (HAL_GPIO_ReadPin(BOOT_BUTTON_PORT, BOOT_BUTTON_PIN) == GPIO_PIN_RESET)
        return 1;

    uint32_t app_reset_vector = *(volatile uint32_t *)(BOOT_APP_ADDR + 4);
    if (app_reset_vector < 0x08000000 || app_reset_vector > BOOT_FLASH_END)
        return 1;

    return 0;
}

static void jump_to_app(void)
{
    uint32_t app_msp   = *(volatile uint32_t *)(BOOT_APP_ADDR);
    uint32_t app_reset = *(volatile uint32_t *)(BOOT_APP_ADDR + 4);

    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    SCB->ICSR |= SCB_ICSR_PENDSTCLR_Msk;

    __set_MSP(app_msp);
    ((void (*)(void))app_reset)();
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  if (!should_stay_in_bootloader())
  {
    jump_to_app();
  }

  CRC32_Init();
  MX_USB_DEVICE_Init();

  uint32_t last_blink = HAL_GetTick();

  while (1)
  {
    Boot_ProcessUSBCommands();

    if (HAL_GetTick() - last_blink >= 500)
    {
      HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);
      last_blink = HAL_GetTick();
    }
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

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

  /* Select HSI48 as USB clock source (default after reset is PLL = 32MHz, not 48MHz) */
  MODIFY_REG(RCC->CCIPR, RCC_CCIPR_HSI48SEL, RCC_CCIPR_HSI48SEL);
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* LED1 (PA0) */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Boot button (PB11, active LOW with pull-up) */
  GPIO_InitStruct.Pin = BOOT_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BOOT_BUTTON_PORT, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
