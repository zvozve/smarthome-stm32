#ifndef __HLK_RM58S_DRIVER_H__
#define __HLK_RM58S_DRIVER_H__

#include "hlk_rm58s_port.h"
#include "hlk_rm58s_config.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

// ===========================
// 缓冲区大小
// ===========================
#define HLK_UART_BUF_LEN    1024

// ===========================
// 驱动状态枚举
// ===========================
typedef enum {
    HLK_IDLE = 0,
    HLK_BUSY_TX,
    HLK_BUSY_RX,
    HLK_ERROR
} hlk_status_t;

// ===========================
// 驱动任务结构体
// ===========================
typedef struct {
    uint8_t     txBuf[HLK_UART_BUF_LEN];
    uint16_t    txLen;
    uint8_t     rxBuf[HLK_UART_BUF_LEN];
    uint16_t    rxLen;
    hlk_status_t status;
    
    void (*on_rx_complete)(uint8_t *data, uint16_t len);
    void (*on_tx_complete)(void);
    void (*on_error)(void);
} hlk_task_t;

extern hlk_task_t hlk_task;

// ===========================
// API
// ===========================
void HLK_Driver_Init(void);
int HLK_Driver_Send(const uint8_t *data, uint16_t len);
void HLK_Driver_RegisterCallbacks(
    void (*on_rx)(uint8_t *data, uint16_t len),
    void (*on_tx)(void),
    void (*on_err)(void)
);

// ===========================
// GPIO
// ===========================
void HLK_GPIO_Init(void);
void HLK_GPIO_EnterATMode(void);
void HLK_GPIO_ActiveReset(void);
void HLK_GPIO_ActiveRecoveryMode(void);

// HAL 回调
// ===========================
void HLK_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HLK_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

#endif