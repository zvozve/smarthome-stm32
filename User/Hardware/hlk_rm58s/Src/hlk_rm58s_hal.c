#include "hlk_rm58s.h"
#include "stm32f4xx_hal.h"
#include "gpio.h"  // CubeMX生成的GPIO定义

// ===========================
// GPIO引脚定义（使用CubeMX宏）
// ===========================

// 这些宏在 gpio.h 中定义：
// WIFI_ES_GPIO_Port, WIFI_ES_Pin
// WIFI_RST_GPIO_Port, WIFI_RST_Pin
// WIFI_STS_GPIO_Port, WIFI_STS_Pin
// WIFI_SOT_GPIO_Port, WIFI_SOT_Pin

// ===========================
// UART适配函数
// ===========================

int hal_uart_send(void *handle, uint8_t *data, uint16_t len) {
    UART_HandleTypeDef *huart = (UART_HandleTypeDef*)handle;
    return HAL_UART_Transmit_DMA(huart, data, len);
}

int hal_uart_recv_start(void *handle, uint8_t *buf, uint16_t len) {
    HLK_LOG("uart_recv_start len=%d\n", len);
    UART_HandleTypeDef *huart = (UART_HandleTypeDef*)handle;
    
    // 先停止之前的 DMA
    HAL_UART_DMAStop(huart);
    
    // 清除空闲中断标志
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    
    int ret = HAL_UARTEx_ReceiveToIdle_DMA(huart, buf, len);
    HLK_LOG("HAL_UARTEx_ReceiveToIdle_DMA returned: %d\n", ret);
    return ret;
}

void hal_uart_abort(void *handle) {
    UART_HandleTypeDef *huart = (UART_HandleTypeDef*)handle;
    HAL_UART_Abort(huart);
}

// ===========================
// GPIO便捷函数（供hlk_rm58s.c内部使用，封装CubeMX宏）
// ===========================

void hal_gpio_write_es(uint8_t level) {
    HAL_GPIO_WritePin(WIFI_ES_GPIO_Port, WIFI_ES_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HLK_LOG("hal_gpio_write_es level: %d\r\n", level);
}

void hal_gpio_write_rst(uint8_t level) {
    HAL_GPIO_WritePin(WIFI_RST_GPIO_Port, WIFI_RST_Pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HLK_LOG("hal_gpio_write_rst level: %d\r\n", level);
}

uint8_t hal_gpio_read_sts(void) {
    return (HAL_GPIO_ReadPin(WIFI_STS_GPIO_Port, WIFI_STS_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

uint8_t hal_gpio_read_sot(void) {
    return (HAL_GPIO_ReadPin(WIFI_SOT_GPIO_Port, WIFI_SOT_Pin) == GPIO_PIN_SET) ? 1 : 0;
}

// ===========================
// HAL中断回调转发
// ===========================

extern UART_HandleTypeDef huart6;

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart6) {
        hlk_rm58s_uart_tx_complete();
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart == &huart6) {
        hlk_rm58s_uart_rx_complete(huart->pRxBuffPtr, Size);
    }
}