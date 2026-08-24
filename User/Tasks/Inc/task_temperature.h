#ifndef __TASK_TEMPERATURE_H
#define __TASK_TEMPERATURE_H

#include "main.h"
#include "cmsis_os.h"
#include "dht11.h"
#include <stdint.h>

// 温度数据结构（供应用层使用）
typedef struct {
    float temperature;
    float humidity;
    uint8_t is_valid;
    uint32_t timestamp;
    int8_t quality;
} TempData_t;

// 任务配置
#define TEMP_TASK_PERIOD_MS     2000    // 读取周期2秒
#define TEMP_TASK_PRIORITY      osPriorityNormal
#define TEMP_TASK_STACK_SIZE    256

// ============================================
// 任务层 API
// ============================================

extern void Task_Temperature_Init(void);
extern void Task_Temperature_Start(void);

// 数据获取接口（供应用层使用）
extern uint8_t Task_Temperature_GetData(TempData_t *pData);
extern float Task_Temperature_GetValue(void);
extern float Task_Temperature_GetHumidity(void);
extern uint8_t Task_Temperature_IsValid(void);
extern void Task_Temperature_GetString(char *buffer, uint32_t size);

// RTOS 任务函数
extern void Task_Temperature(void *argument);

#endif