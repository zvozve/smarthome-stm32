#ifndef __HLK_RM58S_DRIVER_H__
#define __HLK_RM58S_DRIVER_H__

#include "hlk_rm58s_port.h"
#include "hlk_rm58s_config.h"
#include "hlk_rm58s_at.h"

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
// 状态枚举
// ===========================
typedef enum {
    UTS_IDLE = 0x00,
    UTS_REQUESTING,
    UTS_TX_ING,
    UTS_TX_CPLT,
    UTS_TX_ERR,
    UTS_TX_TIMEOUT,
    UTS_RX_ING,
    UTS_RX_CPLT,
    UTS_RX_ERR,
    UTS_RX_TIMEOUT,
    UTS_PARSING,
    UTS_CALLBACK,
} uart_status_t;

// ===========================
// 驱动任务结构体
// ===========================
typedef struct {
    uint8_t     txBuf[HLK_UART_BUF_LEN];
    uint16_t    txLen;
    uint8_t     rxBuf[HLK_UART_BUF_LEN];
    uint16_t    rxLen;
    uint8_t     rxAccumBuf[HLK_UART_BUF_LEN];
    uint16_t    rxAccumLen;
    uart_status_t status;
    uint32_t    timeout;
    uint8_t     retry;
    bool        is_at_mode;     // true=AT指令模式, false=透传模式
    // 回调函数指针
    void (*on_response_complete)(void);
    void (*on_timeout)(void);
    void (*on_error)(void);
} hlk_task_t;
extern hlk_task_t hlk_task;

// ===========================
// 驱动层 API（App 调用）
// ===========================
void HLK_Driver_Init(void);
void HLK_Driver_RegisterCallbacks(void (*on_resp)(void), void (*on_to)(void), void (*on_err)(void));
void HLK_Driver_Transmit(const char *command);
void HLK_Driver_Poll(void);  // 由定时器中断调用
void HLK_Driver_SetATMode(bool enable);
bool HLK_Driver_GetATMode(void);

// ===========================
// 内部函数（驱动内部使用）
// ===========================
char *strcasestr_local(const char *haystack, const char *needle);
void HLK_GPIO_Init(void);
void HLK_GPIO_ActiveATMode(void);
void HLK_GPIO_ActiveReset(void);
void HLK_GPIO_ActiveRecoveryMode(void);

// HAL 回调（由中断调用）
void HLK_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HLK_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HLK_TIMX_IRQHandler(TIM_HandleTypeDef *htim);

#endif // __HLK_RM58S_DRIVER_H__