/**
  ******************************************************************************
  * @file           : gpio.h
  * @brief          : GPIO driver for FV1 Programmer
  ******************************************************************************
  * @description
  *
  * This module provides GPIO control functions for the FV1 EEPROM Programmer:
  * - LED1 (PA0): Status indicator
  * - LED2 (PA1): Status indicator
  * - Button (PB11): Programming trigger (active LOW)
  * - Switch (PA2): Write lock/unlock
  *
  ******************************************************************************
  */

#ifndef __GPIO_H
#define __GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* Pin Definitions -----------------------------------------------------------*/
#define LED1_GPIO_Port      GPIOA
#define LED1_Pin            GPIO_PIN_0

#define LED2_GPIO_Port      GPIOA
#define LED2_Pin            GPIO_PIN_1

#define BUTTON_GPIO_Port    GPIOB
#define BUTTON_Pin          GPIO_PIN_11

#define SWITCH_GPIO_Port    GPIOA
#define SWITCH_Pin          GPIO_PIN_2

/* Debouncing Configuration --------------------------------------------------*/
#define DEBOUNCE_TIME_MS    50      // Button debounce time in milliseconds
#define LONG_PRESS_TIME_MS  2000    // Long press detection time in milliseconds

/* LED Control Functions -----------------------------------------------------*/

/**
  * @brief  Turn on LED1
  * @retval None
  */
void LED1_On(void);

/**
  * @brief  Turn off LED1
  * @retval None
  */
void LED1_Off(void);

/**
  * @brief  Toggle LED1 state
  * @retval None
  */
void LED1_Toggle(void);

/**
  * @brief  Turn on LED2
  * @retval None
  */
void LED2_On(void);

/**
  * @brief  Turn off LED2
  * @retval None
  */
void LED2_Off(void);

/**
  * @brief  Toggle LED2 state
  * @retval None
  */
void LED2_Toggle(void);

/* Input Reading Functions ---------------------------------------------------*/

/**
  * @brief  Read raw button state (debounced)
  * @retval true if button is pressed (LOW), false otherwise
  */
bool Button_IsPressed(void);

/**
  * @brief  Check if button was just pressed (edge detection)
  * @retval true on button press event, false otherwise
  */
bool Button_WasPressed(void);

/**
  * @brief  Check if button has been held for long press duration
  * @retval true if button held for LONG_PRESS_TIME_MS, false otherwise
  */
bool Button_IsLongPress(void);

/**
  * @brief  Read switch state
  * @retval true if switch is in unlocked position, false if locked
  * @note   Adjust logic based on actual hardware (pull-up/pull-down)
  */
bool Switch_IsUnlocked(void);

/**
  * @brief  Update button debouncing state machine
  * @note   Must be called periodically (e.g., every 1ms from SysTick)
  */
void GPIO_Update(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_H */
