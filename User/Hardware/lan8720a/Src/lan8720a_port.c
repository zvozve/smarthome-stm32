/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : lan8720a_port.c
  * @brief          : Ethernet PHY hardware reset routine
  ******************************************************************************
  */
/* USER CODE END Header */
#include "lan8720a_port.h"

/**
  * @brief  Simple busy-loop millisecond delay (no HAL/scheduler dependency).
  * @note   Core runs at 168 MHz → ~168 cycles/ms with -Os.
  *         Loop calibrated for Cortex-M4 at 168 MHz.
  */
static void delay_ms(uint32_t ms)
{
  for (uint32_t i = 0; i < ms; i++)
  {
    for (volatile uint32_t j = 0; j < 42000; j++) { }
  }
}

/**
  * @brief  Perform hardware reset of the Ethernet PHY (LAN8742).
  * @note   Assert reset low for 10ms, then release and wait 30ms
  *         for the PHY to stabilize. Call before MX_LWIP_Init().
  */
void ETH_PHY_HardwareReset(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Enable GPIO clock for the reset pin port */
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Configure reset pin as output */
  GPIO_InitStruct.Pin = ETH_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ETH_RST_GPIO_Port, &GPIO_InitStruct);

  /* Assert reset (active low) */
  HAL_GPIO_WritePin(ETH_RST_GPIO_Port, ETH_RST_Pin, GPIO_PIN_RESET);
  delay_ms(50);

  /* De-assert reset */
  HAL_GPIO_WritePin(ETH_RST_GPIO_Port, ETH_RST_Pin, GPIO_PIN_SET);
  delay_ms(10);
}
