#include "task_temperature.h"
#include "dht11.h"
#include "SEGGER_RTT_Log.h"
#include "bsp_dwt.h"
#include <string.h>
#include <stdio.h>

static osThreadId_t g_temp_task_handle = NULL;

static TempData_t g_temp_data = {
    .temperature = 0.0f,
    .humidity = 0.0f,
    .is_valid = 0,
    .timestamp = 0,
    .quality = 0
};

void Task_Temperature_Init(void)
{
    DHT11_Init();
    Task_Temperature_Start();
}

void Task_Temperature_Start(void)
{
    if (g_temp_task_handle != NULL) {
        TEMP_LOG("Temperature Task Already Running!\r\n");
        return;
    }
    
    const osThreadAttr_t temp_task_attr = {
        .name = "TemperatureTask",
        .stack_size = TEMP_TASK_STACK_SIZE * 4,
        .priority = TEMP_TASK_PRIORITY,
    };
    
    g_temp_task_handle = osThreadNew(Task_Temperature, NULL, &temp_task_attr);
    
    if (g_temp_task_handle != NULL) {
        TEMP_LOG("Temperature Task Started Successfully!\r\n");
    } else {
        TEMP_LOG("Temperature Task Create Failed!\r\n");
    }
}

void Task_Temperature(void *argument)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    DHT11_Data_t dht_data;
    uint32_t count = 0;
    
    vTaskDelay(pdMS_TO_TICKS(500));
    TEMP_LOG("Temperature Task Running...\r\n");
    
    for (;;)
    {
        if (DHT11_Read(&dht_data) && dht_data.is_valid)
        {
            g_temp_data.temperature = (float)dht_data.temp_int + (float)dht_data.temp_dec * 0.1f;
            g_temp_data.humidity = (float)dht_data.hum_int + (float)dht_data.hum_dec * 0.1f;
            g_temp_data.is_valid = 1;
            g_temp_data.quality = 1;
            g_temp_data.timestamp = HAL_GetTick();
            
            if (++count % 5 == 0) {
                TEMP_LOG("[Task] T=%.1f°C, H=%.1f%%\r\n", 
                         g_temp_data.temperature, g_temp_data.humidity);
            }
        }
        else
        {
            g_temp_data.is_valid = 0;
            g_temp_data.quality = -1;
            TEMP_LOG("[Task] Read failed\r\n");
        }
        
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(TEMP_TASK_PERIOD_MS));
    }
}