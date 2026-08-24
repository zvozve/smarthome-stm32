#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include "cmsis_os2.h"
#include "SEGGER_RTT.h"
#include "task_mqtt.h"
#include "task_wol.h"      // ← 添加

// ===========================
// 固件版本
// ===========================
#define FIRMWARE_VERSION  "V1.0.1/2026-07-15"

// ===========================
// 任务句柄
// ===========================
extern osThreadId_t appTaskHandle;
extern osThreadId_t wifiTaskHandle;
extern osThreadId_t wifiPollTaskHandle;
extern osThreadId_t mqttTaskHandle;
extern osThreadId_t wolTaskHandle;   // ← 添加

// ===========================
// 函数声明
// ===========================
void app_main_init(void);
void app_main_create_tasks(void);

#endif /* __APP_MAIN_H */