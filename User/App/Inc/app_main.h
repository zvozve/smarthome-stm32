#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include "cmsis_os2.h"
#include "SEGGER_RTT.h"

// ===========================
// 调试宏
// ===========================

#define APP_DEBUG_ENABLE    1

#if APP_DEBUG_ENABLE
    #define APP_LOG(fmt, ...)   SEGGER_RTT_printf(0, "[APP] " fmt, ##__VA_ARGS__)
#else
    #define APP_LOG(fmt, ...)   ((void)0)
#endif

// ===========================
// 任务句柄
// ===========================

extern osThreadId_t wolTaskHandle;
extern osThreadId_t wifiTaskHandle;
extern osThreadId_t wifiPollTaskHandle;
extern osThreadId_t mqttTaskHandle;

// ===========================
// 函数声明
// ===========================

void app_main_init(void);
void app_main_create_tasks(void);

#endif /* __APP_MAIN_H */