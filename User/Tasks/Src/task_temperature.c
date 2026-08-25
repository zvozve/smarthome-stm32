/**
 * @file    task_temperature.c
 * @brief   温度采集任务实现
 * @version V2.0
 * @date    2026-08-25
 */

#include "task_temperature.h"
#include "dht11.h"
#include "SEGGER_RTT_Log.h"
#include "bsp_dwt.h"
#include <string.h>

/* ========== 静态变量 ========== */

static bool g_initialized = false;
static TaskHandle_t g_task_handle = NULL;
static osMutexId_t g_mutex = NULL;

/* 数据缓存 */
static TempData_t g_data = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .is_valid = false,
    .timestamp = 0
};

/* 失败计数（防刷屏） */
static uint32_t g_fail_count = 0;

/* ========== 内部函数 ========== */

static void update_data(const DHT11_Data_t *pDhtData)
{
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }

    if (pDhtData != NULL && pDhtData->is_valid) {
        g_data.temperature = (float)pDhtData->temp_int + (float)pDhtData->temp_dec * 0.1f;
        g_data.humidity    = (float)pDhtData->hum_int + (float)pDhtData->hum_dec * 0.1f;
        g_data.is_valid    = true;
        g_data.timestamp   = HAL_GetTick();
        g_fail_count = 0;
    } else {
        g_data.is_valid = false;
        g_fail_count++;
    }

    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }
}

static void print_log(const DHT11_Data_t *pData)
{
    static uint32_t print_cnt = 0;
    print_cnt++;

    if (pData != NULL && pData->is_valid) {
        if (print_cnt % 5 == 0) {
            // 使用整数打印，避免浮点 printf 问题
            DBG_LOG("[Temp] T=%d.%dC, H=%d.%d%%",
                    pData->temp_int, pData->temp_dec,
                    pData->hum_int,  pData->hum_dec);
        }
    } else {
        if (g_fail_count <= 3 || g_fail_count % 10 == 0) {
            DBG_LOG("[Temp] Read failed (count=%lu)", g_fail_count);
        }
    }
}

/* ========== 任务入口 ========== */

static void task_temperature_entry(void *arg)
{
    (void)arg;

    TickType_t last_wake = xTaskGetTickCount();
    DHT11_Data_t dht_data;

    DHT11_Init();
    DBG_LOG("[Temp] Task started");
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    while (1) {
        bool ok = DHT11_Read(&dht_data);

        if (ok && dht_data.is_valid) {
            update_data(&dht_data);
        } else {
            update_data(NULL);
        }

        print_log(ok ? &dht_data : NULL);

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS));
    }
}

/* ========== 对外 API ========== */

bool Task_Temperature_Init(void)
{
    /* 创建互斥量 */
    g_mutex = osMutexNew(NULL);
    if (g_mutex == NULL) {
        DBG_LOG("[Temp] Mutex create failed");
        /* 继续运行，只是没有保护 */
    }

    if (xTaskCreate(task_temperature_entry, TEMP_TASK_NAME,
                    TEMP_TASK_STACK_SIZE, NULL,
                    TEMP_TASK_PRIORITY, &g_task_handle) != pdPASS) {
        DBG_LOG("[Temp] Task create failed");
        return false;
    }

    g_initialized = true;
    DBG_LOG("[Temp] Task created");
    return true;
}

bool Task_Temperature_GetData(TempData_t *pData)
{
    if (pData == NULL || !g_initialized) {
        return false;
    }

    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
        memcpy(pData, &g_data, sizeof(TempData_t));
        osMutexRelease(g_mutex);
    } else {
        memcpy(pData, &g_data, sizeof(TempData_t));
    }

    return pData->is_valid;
}