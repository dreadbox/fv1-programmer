/**
  ******************************************************************************
  * @file           : gpio.c
  * @brief          : GPIO driver implementation for FV1 Programmer
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "gpio.h"

/* Private Variables ---------------------------------------------------------*/
static struct {
    bool current_state;      // Current debounced button state
    bool previous_state;     // Previous debounced state for edge detection
    bool raw_state;          // Raw button reading
    uint32_t debounce_timer; // Debounce timer counter (in ms)
    uint32_t press_timer;    // Press duration timer (in ms)
    bool long_press_flag;    // Flag indicating long press detected
} button_state = {0};

/* LED Control Functions -----------------------------------------------------*/

/**
  * @brief  Turn on LED1
  */
void LED1_On(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
}

/**
  * @brief  Turn off LED1
  */
void LED1_Off(void)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  Toggle LED1 state
  */
void LED1_Toggle(void)
{
    HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
}

/**
  * @brief  Turn on LED2
  */
void LED2_On(void)
{
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
}

/**
  * @brief  Turn off LED2
  */
void LED2_Off(void)
{
    HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
}

/**
  * @brief  Toggle LED2 state
  */
void LED2_Toggle(void)
{
    HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin);
}

/* Input Reading Functions ---------------------------------------------------*/

/**
  * @brief  Read raw button state (debounced)
  * @retval true if button is pressed (LOW), false otherwise
  */
bool Button_IsPressed(void)
{
    return button_state.current_state;
}

/**
  * @brief  Check if button was just pressed (edge detection)
  * @retval true on button press event, false otherwise
  */
bool Button_WasPressed(void)
{
    if (button_state.current_state && !button_state.previous_state) {
        return true;
    }
    return false;
}

/**
  * @brief  Check if button has been held for long press duration
  * @retval true if button held for LONG_PRESS_TIME_MS, false otherwise
  */
bool Button_IsLongPress(void)
{
    return button_state.long_press_flag;
}

/**
  * @brief  Read switch state
  * @retval true if switch is in unlocked position, false if locked
  * @note   With pull-up: LOW = unlocked, HIGH = locked
  */
bool Switch_IsUnlocked(void)
{
    // Read the switch pin (with pull-up, LOW = switch closed/unlocked)
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(SWITCH_GPIO_Port, SWITCH_Pin);

    // Return true if switch is LOW (unlocked)
    return (pin_state == GPIO_PIN_RESET);
}

/**
  * @brief  Update button debouncing state machine
  * @note   Must be called periodically (e.g., every 1ms from SysTick)
  */
void GPIO_Update(void)
{
    // Read raw button state (active LOW with pull-up)
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(BUTTON_GPIO_Port, BUTTON_Pin);
    bool current_raw = (pin_state == GPIO_PIN_RESET);

    // Debouncing logic
    if (current_raw != button_state.raw_state) {
        // Button state changed, reset debounce timer
        button_state.raw_state = current_raw;
        button_state.debounce_timer = 0;
    } else {
        // Button state stable, increment timer
        button_state.debounce_timer++;

        // Check if debounce time has elapsed
        if (button_state.debounce_timer >= DEBOUNCE_TIME_MS) {
            // Only update if state actually changed
            if (button_state.current_state != button_state.raw_state) {
                button_state.previous_state = button_state.current_state;
                button_state.current_state = button_state.raw_state;
            }

            // Reset debounce timer to prevent overflow
            button_state.debounce_timer = DEBOUNCE_TIME_MS;
        }
    }

    // Long press detection
    if (button_state.current_state) {
        // Button is pressed, increment press timer
        button_state.press_timer++;

        // Check for long press
        if (button_state.press_timer >= LONG_PRESS_TIME_MS) {
            button_state.long_press_flag = true;
            button_state.press_timer = LONG_PRESS_TIME_MS; // Prevent overflow
        }
    } else {
        // Button released, reset timers and flags
        button_state.press_timer = 0;
        button_state.long_press_flag = false;
    }
}
