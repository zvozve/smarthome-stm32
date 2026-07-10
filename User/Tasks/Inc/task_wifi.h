#ifndef __TASK_WIFI_H
#define __TASK_WIFI_H

#include <stdbool.h>
#include "SEGGER_RTT.h"

// ===========================
// WiFi任务日志宏
// ===========================

#define WIFI_LOG_ENABLE     1

#if WIFI_LOG_ENABLE
    #define WIFI_LOG(fmt, ...)   SEGGER_RTT_printf(0, "[WiFi] " fmt, ##__VA_ARGS__)
#else
    #define WIFI_LOG(fmt, ...)   ((void)0)
#endif

// ===========================
// 函数声明
// ===========================

/**
 * @brief WiFi任务函数（由osThreadNew创建）
 */
void task_wifi(void *argument);

/**
 * @brief 获取WiFi连接状态
 */
bool task_wifi_is_connected(void);

/**
 * @brief 获取TCP连接状态
 */
bool task_wifi_is_tcp_connected(void);

#endif /* __TASK_WIFI_H */