#ifndef __HLK_RM58S_PORT_H
#define __HLK_RM58S_PORT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "string.h"
#include <math.h>
#include "stdarg.h"	 
#include <ctype.h>

#include "main.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"
#include "dma.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
#include "SEGGER_RTT.h"

/*
***********************************************
**  GPIO CONFIG
***********************************************
*/

// ES0引脚（进入 AT 模式 / 恢复出厂）
#define WIFI_ES_LOW()      HAL_GPIO_WritePin(WIFI_ES_GPIO_Port, WIFI_ES_Pin, GPIO_PIN_RESET)
#define WIFI_ES_HIGH()     HAL_GPIO_WritePin(WIFI_ES_GPIO_Port, WIFI_ES_Pin, GPIO_PIN_SET)

// 模块复位，低电平有效，保持≥500ms
#define WIFI_RST_LOW()     HAL_GPIO_WritePin(WIFI_RST_GPIO_Port, WIFI_RST_Pin, GPIO_PIN_RESET)
#define WIFI_RST_HIGH()    HAL_GPIO_WritePin(WIFI_RST_GPIO_Port, WIFI_RST_Pin, GPIO_PIN_SET)

// WIFI连接状态（高=已连接）
#define WIFI_STS_READ()    HAL_GPIO_ReadPin(WIFI_STS_GPIO_Port, WIFI_STS_Pin)

// SOCKET连接状态（高=已连接）
#define WIFI_SOT_READ()    HAL_GPIO_ReadPin(WIFI_SOT_GPIO_Port, WIFI_SOT_Pin)

// #define WIFI_LINK_ON()    HAL_GPIO_WritePin(WIFI_LINK_GPIO_Port, WIFI_LINK_Pin, GPIO_PIN_RESET)
// #define WIFI_LINK_OFF()   HAL_GPIO_WritePin(WIFI_LINK_GPIO_Port, WIFI_LINK_Pin, GPIO_PIN_SET)

// #define WIFI_RX_TX_ON()   HAL_GPIO_WritePin(WIFI_RX_TX_GPIO_Port, WIFI_RX_TX_Pin, GPIO_PIN_RESET)
// #define WIFI_RX_TX_OFF()  HAL_GPIO_WritePin(WIFI_RX_TX_GPIO_Port, WIFI_RX_TX_Pin, GPIO_PIN_SET)

/*
***********************************************
**  TIM CONFIG
***********************************************
*/

#define TIM_HLK              TIM6
#define htim_hlk             htim6
extern TIM_HandleTypeDef     htim_hlk;

#define TIM_HLK_PERIOD_US    (1000)
#define TIM_HLK_MAX_DELAY    (1000 * 1000 / TIM_HLK_PERIOD_US)
#define TIM_HLK_MAX_RETRY    (3)

extern volatile uint32_t us_count;

void TIM_HLK_Delay_us(uint16_t us);
void TIM_HLK_Delay_ms(uint16_t ms);

/*
***********************************************
**  USART CONFIG
***********************************************
*/

#define UART_ENABLE_DMACPY      0
#define UART_ENABLE_RTT_PRINTF  1
#define UART_BUF_LEN 1024

#define UART_HLK               USART6
#define huart_hlk              huart6
extern UART_HandleTypeDef       huart_hlk;

typedef enum {
    UTS_IDLE = 0x81,    // 空闲

    UTS_TX_ENTRY,       // 发送
    UTS_TX_ING,         // 发送进行
    UTS_TX_CPLT,        // 发送完成
    UTS_TX_ERR,         // 发送出错
    UTS_TX_TIMEOUT,     // 发送超时
    UTS_RX_ENTRY,       // 接收
    UTS_RX_ING,         // 接收进行
    UTS_RX_CPLT,        // 接收完成
    UTS_RX_ERR,         // 接收出错
    UTS_RX_TIMEOUT,     // 接收超时

    UTS_REQUESTING,     // 请求
    UTS_RESPONDING,     // 响应
    UTS_PARSING,        // 解析
    UTS_CALLBACK,       // 回调
} uart_Status_e;

// 定义函数指针类型
typedef struct
{
    UART_HandleTypeDef  *Port;
    uint8_t             Tx_Buf[UART_BUF_LEN];
    uint16_t            Tx_Byte;
    uint8_t             Rx_Buf[UART_BUF_LEN];
    uint16_t            Rx_Byte;
    uart_Status_e       status;
    uint32_t            timeout; // 超时时间(ms)
    uint8_t             retry;   // 重试计数器
    // 添加RS485控制函数指针
    void (*RS485_TX_Mode)(void);
    void (*RS485_RX_Mode)(void);
    
    // 添加调试标识
    const char *debug_name;
}Uart_Task_t;
extern Uart_Task_t              Uart_Task_WiFi;

#if UART_ENABLE_DMACPY
/**
 * @brief   DMA搬运
 */
#define hdma_mb_cpy         hdma_memtomem_dma2_channel4
extern DMA_HandleTypeDef    hdma_mb_cpy;

typedef struct {
    uint16_t index;
    uint16_t cnt;
    uint16_t move_len;
    uint16_t *p_data;
    uint8_t step;
} FIFO_DMA_Context_t;
#endif // UART_ENABLE_DMACPY

/*
***********************************************
**  FUNC
***********************************************
*/

void USR_UART_Transmit_DMA(Uart_Task_t *task);
void USR_UART_Receive_DMA(Uart_Task_t *task);
void USR_UART_TxCpltCallback(Uart_Task_t *task);
void USR_UARTEx_RxEventCallback(Uart_Task_t *task, uint16_t Size);

void USR_UART_Print_CMD(const char *idFormat, const char *sFormat, uint8_t *pCmd, uint16_t len);

uint8_t USR_UART_Unstable(UART_HandleTypeDef *huart);
void USR_UART_Repair(UART_HandleTypeDef *huart);

#endif // __HLK_RM58S_PORT_H
