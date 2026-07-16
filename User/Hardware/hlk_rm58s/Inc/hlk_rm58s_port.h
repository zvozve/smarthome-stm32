#ifndef __HLK_RM58S_PORT_H
#define __HLK_RM58S_PORT_H

#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

// ===========================
// 驱动版本
// ===========================
#define HLK_DRIVER_VERSION  "V2.0.0/2026-07-16"

// ===========================
// RTOS开关（放在这里）
// ===========================
#define HLK_USE_RTOS       1    // 0: 裸机, 1: RTOS

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

#if HLK_USE_RTOS
    #define HLK_GET_TICK()      xTaskGetTickCount()
    #define HLK_Delay_ms(ms)    vTaskDelay(pdMS_TO_TICKS(ms))
#else
    #define HLK_GET_TICK()      HAL_GetTick()
    #define HLK_Delay_ms(ms)    HAL_Delay(ms)
#endif

// ===========================
// RTT 日志（带时间戳）
// ===========================
#ifndef HLK_LOG_ENABLE
    #define HLK_LOG_ENABLE     1
#endif

#if HLK_LOG_ENABLE
    #include "SEGGER_RTT.h"
    #define HLK_LOG(fmt, ...) \
        do { \
            uint32_t _tick = HLK_GET_TICK(); \
            SEGGER_RTT_printf(0, "[%lu][HLK] " fmt, _tick, ##__VA_ARGS__); \
        } while(0)
#else
    #define HLK_LOG(fmt, ...)   ((void)0)
#endif

// ===========================
// GPIO 引脚宏
// ===========================

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

// ===========================
// 串口/定时器 句柄
// ===========================
#define huart_hlk           huart6
#define htim_hlk            htim6

extern UART_HandleTypeDef   huart_hlk;
extern TIM_HandleTypeDef    htim_hlk;

#endif // __HLK_RM58S_PORT_H