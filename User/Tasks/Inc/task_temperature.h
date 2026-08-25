/**
 * @file    task_temperature.h
 * @brief   温度采集任务
 * @version V2.0
 * @date    2026-08-25
 */

#ifndef __TASK_TEMPERATURE_H
#define __TASK_TEMPERATURE_H

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>

/* ========== 数据结构 ========== */

typedef struct {
    float    temperature;   /* 温度，单位 °C */
    float    humidity;      /* 湿度，单位 % */
    bool     is_valid;      /* 数据是否有效 */
    uint32_t timestamp;     /* 采集时间戳，ms */
} TempData_t;

/* ========== 任务配置 ========== */

#define TEMP_TASK_NAME          "TemperatureTask"
#define TEMP_TASK_PERIOD_MS     2000
#define TEMP_TASK_PRIORITY      osPriorityNormal
#define TEMP_TASK_STACK_SIZE    256

/* ========== API ========== */

bool Task_Temperature_Init(void);
bool Task_Temperature_GetData(TempData_t *pData);

#endif /* __TASK_TEMPERATURE_H */