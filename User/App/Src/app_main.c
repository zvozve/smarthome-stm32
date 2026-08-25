/**
 * @file    app_main.c
 * @brief   应用入口 - 任务管理 (WiFi / MQTT)
 */

#include "app_main.h"
#include "task_wifi.h"
#include "task_mqtt.h"
#include "bsp_dwt.h"
#include "heart_beat.h"
#include "lan8720a_reset.h"
#include "SEGGER_RTT_Log.h"

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

    if (Task_WiFi_init()) {
        APP_LOG("WiFi task created");
    } else {
        APP_LOG("ERROR: WiFi task create failed");
    }

    /* MQTT 任务依赖 WiFi 任务提供的收发接口；先建 WiFi，再启 MQTT */
    Task_MQTT_Init();
    APP_LOG("MQTT task init done");

    /* WoL 已内联到 business_mqtt（业务层），无独立任务 */

    APP_LOG("All tasks created");
}

/* ===========================
 * 默认任务
 * =========================== */
void app_main_task(void) {
    osDelay(1000);
    APP_LOG("Default task started (idle)");
    heart_beat_init();

    while (1) {
        static uint32_t last_toggle = 0;
        uint32_t now = osKernelGetTickCount();
        
        if (now - last_toggle >= 500) {
            last_toggle = now;
            heart_beat_run();
        }

        osDelay(10);
    }
}