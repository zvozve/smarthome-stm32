/**
 * @file    app_main.c
 * @brief   应用入口 - 任务管理 (WiFi / MQTT)
 */

#include "app_main.h"

#include "board_cfg.h"     /* 工程侧硬件绑定（引脚/句柄，类 devicetree） */
#include "oop_dwt.h"

#include "heart_beat.h"
#include "lan8720a_reset.h"

#include "task_wifi.h"
#include "task_mqtt.h"
#include "task_temperature.h"
#include "task_ir.h"

/* ===========================
 * 应用初始化
 * =========================== */
void app_main_init(void) {
    oop_InitDWT();
    board_io_init();      /* 非 CubeMX 管理的外设时钟（TIM9 载波定时器） */
    ETH_RST_Init(BOARD_ETH_RST_PORT, BOARD_ETH_RST_PIN);
    SEGGER_RTT_Init();
    APP_LOG("========================================");
    APP_LOG("System initializing...");
    APP_LOG("FIRMWARE_VERSION: %s", FIRMWARE_VERSION);

    /* 注意：PE5 的中断回调由 IR1838B_Init 统一注册，
     * 不要在这里重复注册（同一引脚后注册会覆盖先注册者）。 */

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

    // Task_MQTT_Init();
    // APP_LOG("MQTT task init done");

    // Task_Temperature_Init();
    // APP_LOG("Temperature task init done");

    /* IR 任务：1838B 接收（TIM6 静默收尾）+ 红外发射（TIM9 载波） */
    if (Task_IR_Init()) {
        APP_LOG("IR task created");
    } else {
        APP_LOG("ERROR: IR task create failed");
    }

    APP_LOG("All tasks created");
}

/* ===========================
 * 默认任务（GPIO 翻转测试）
 * =========================== */
void app_main_task(void) {
    osDelay(500);

    APP_LOG("Default task started");
    heart_beat_init(BOARD_HEART_LED_PORT, BOARD_HEART_LED_PIN, BOARD_HEART_IWDG);

    while (1) {
        /* 心跳 */
        static uint32_t last_toggle = 0;
        uint32_t now = osKernelGetTickCount();
        if (now - last_toggle >= 500) {
            last_toggle = now;
            heart_beat_run();
        }

        /* 周期性打印引脚电平状态 */
        static uint32_t last_print = 0;
        if (now - last_print >= 3000) {
            last_print = now;
            uint8_t level = HAL_GPIO_ReadPin(BOARD_IR_RX_PORT, BOARD_IR_RX_PIN);
            APP_LOG("IR pin level: %d (idle should be 1)", level);
        }

        osDelay(100);
    }
}