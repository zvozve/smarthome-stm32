/**
 * @file    app_main.c
 * @brief   应用入口 - 任务管理 (WiFi / MQTT)
 */

#include "app_main.h"
#include "task_wifi.h"
#include "task_mqtt.h"
#include "bsp_dwt.h"
#include "bsp_gpio_drv.h"
#include "SEGGER_RTT_Log.h"
#include "gpio.h"   /* ★ CubeMX 生成的 GPIO 定义 */

/* ===========================
 * 任务句柄
 * =========================== */
osThreadId_t appTaskHandle;
osThreadId_t wifiTaskHandle;

/* ===========================
 * CPU 状态 LED 设备
 * =========================== */
static gpio_dev_t g_cpu_led_dev;

/* ===========================
 * 硬件初始化（由 CubeMX 生成）
 * =========================== */
extern void SystemClock_Config(void);
extern void MX_DMA_Init(void);
extern void MX_USART6_UART_Init(void);
extern void MX_CRC_Init(void);

/* ===========================
 * 应用初始化
 * =========================== */
void app_main_init(void) {
    SEGGER_RTT_Init();
    APP_LOG("========================================");
    APP_LOG("System initializing...");
    APP_LOG("FIRMWARE_VERSION: %s", FIRMWARE_VERSION);
    APP_LOG("========================================");

    bsp_InitDWT();

    /* ★ 直接用 CubeMX 生成的宏 ★ */
    bsp_gpio_init(&g_cpu_led_dev, CPU_STA_GPIO_Port, CPU_STA_Pin, true);
    bsp_gpio_set_low(&g_cpu_led_dev);

    app_main_create_tasks();

    APP_LOG("App main init done");
    APP_LOG("========================================");
}

/* ===========================
 * 创建所有任务
 * =========================== */
void app_main_create_tasks(void) {
    APP_LOG("app_main_create_tasks");

    if (task_wifi_init()) {
        APP_LOG("WiFi task created");
    } else {
        APP_LOG("ERROR: WiFi task create failed");
    }

    /* MQTT 任务依赖 WiFi 任务提供的收发接口；先建 WiFi，再启 MQTT */
    MQTT_Task_Init();
    APP_LOG("MQTT task init done");

    APP_LOG("All tasks created");
}

/* ===========================
 * 默认任务
 * =========================== */
void appTask(void *argument) {
    (void)argument;
    osDelay(1000);
    APP_LOG("Default task started (idle)");

    while (1) {
        static uint32_t last_toggle = 0;
        uint32_t now = osKernelGetTickCount();
        
        if (now - last_toggle >= 500) {
            last_toggle = now;
            bsp_gpio_toggle(&g_cpu_led_dev);
        }

        static uint32_t last_log = 0;
        if (now - last_log >= 10000) {
            last_log = now;
            APP_LOG("WiFi: %s", task_wifi_is_connected() ? "CONNECTED" : "DISCONNECTED");
        }

        osDelay(10);
    }
}