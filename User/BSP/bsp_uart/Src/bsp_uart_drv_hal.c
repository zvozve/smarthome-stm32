/**
 * @file    bsp_uart_drv_hal.c
 * @brief   HAL 回调转发层
 */

#include "bsp_uart_drv.h"
#include "SEGGER_RTT_Log.h"

/* 所有 HAL 回调统一桥接到驱动层 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    uart_drv_on_tx_done(huart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    uart_drv_on_idle(huart, Size);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    DBG_LOG("UART error: %s", uart_drv_get_name(huart));
    uart_drv_on_error(huart);
}