/**
 * @file    app_main.c
 * @brief   应用入口 - 任务管理 (WiFi / MQTT)
 */

#include "app_main.h"
#include "task_temperature.h"
#include "task_wifi.h"
#include "task_mqtt.h"
#include "bsp_dwt.h"
#include "bsp_gpio_drv.h"          // 新增
#include "heart_beat.h"
#include "lan8720a_reset.h"
#include "SEGGER_RTT_Log.h"
#include "main.h"                  // 确保有 DHT11_DAT_GPIO_Port / Pin

/* ===========================
 * 应用初始化
 * =========================== */
void app_main_init(void) {    
    bsp_InitDWT();
    ETH_RST_Init();
    SEGGER_RTT_Init();
    APP_LOG("========================================");
    APP_LOG("System initializing...");
    APP_LOG("FIRMWARE_VERSION: %s", FIRMWARE_VERSION);

    app_main_create_tasks();

    APP_LOG("App main init done");
    APP_LOG("========================================");
}

/* ===========================
 * 创建所有任务
 * =========================== */
void app_main_create_tasks(void) {
    APP_LOG("app_main_create_tasks");

    // if (Task_WiFi_init()) {
    //     APP_LOG("WiFi task created");
    // } else {
    //     APP_LOG("ERROR: WiFi task create failed");
    // }

    // /* MQTT 任务依赖 WiFi 任务提供的收发接口；先建 WiFi，再启 MQTT */
    // Task_MQTT_Init();
    // APP_LOG("MQTT task init done");

    /* 临时禁用温度任务，用于 GPIO 翻转测试 */
    Task_Temperature_Init();
    APP_LOG("Temperature task init done");

    APP_LOG("All tasks created");
}

/* ===========================
 * 默认任务（GPIO 翻转测试）
 * =========================== */
void app_main_task(void) {
    osDelay(500);

    APP_LOG("Default task started");
    heart_beat_init();

    while (1) {
        /* 心跳 */
        static uint32_t last_toggle = 0;
        uint32_t now = osKernelGetTickCount();
        if (now - last_toggle >= 500) {
            last_toggle = now;
            heart_beat_run();
        }

        osDelay(500);   // 200ms 翻转一次，方便示波器观察（频率 2.5Hz）
    }
}