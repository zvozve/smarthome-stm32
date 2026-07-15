#include "app_main.h"
// Hardware
#include "lan8720a_port.h"
#include "hlk_rm58s_client.h" 
// Protocols
#include "wol.h"
// Tasks
#include "task_wifi.h"
#include "task_mqtt.h"

// ===========================
// 任务句柄定义
// ===========================

osThreadId_t appTaskHandle;
osThreadId_t wifiTaskHandle;
osThreadId_t wifiPollTaskHandle;
osThreadId_t mqttTaskHandle;

// ===========================
// 任务属性定义
// ===========================

// // 默认任务（WOL）
// static const osThreadAttr_t appTask_attrs = {
//     .name = "appTask",
//     .stack_size = 256 * 4,
//     .priority = (osPriority_t) osPriorityNormal,
// };

// WiFi管理任务
static const osThreadAttr_t wifiTask_attrs = {
    .name = "wifiTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t) osPriorityNormal3,
};

// // MQTT任务
// static const osThreadAttr_t mqttTask_attrs = {
//     .name = "mqttTask",
//     .stack_size = 1024 * 4,
//     .priority = (osPriority_t) osPriorityNormal2,
// };

// ===========================
// 函数声明
// ===========================

void appTask(void *argument);

// ===========================
// 硬件初始化
// ===========================

// ===========================
// 应用初始化
// ===========================

void app_main_init(void)
{
    SEGGER_RTT_Init();
    APP_LOG("System initializing...\n");
    APP_LOG("FIRMWARE_VERSION %s\n", FIRMWARE_VERSION);

    // 1. 硬件初始化
    LAN_HardwareReset();

    // 2. 创建所有任务
    app_main_create_tasks();
}

// ===========================
// 创建所有任务
// ===========================

void app_main_create_tasks(void)
{
    // // 默认任务（WOL）
    // appTaskHandle = osThreadNew(appTask, NULL, &appTask_attrs);
    // if (appTaskHandle == NULL) {
    //     APP_LOG("Failed to create appTask\n");
    // } else {
    //     APP_LOG("appTask created\n");
    // }

    // WiFi管理任务
    wifiTaskHandle = osThreadNew(task_wifi, NULL, &wifiTask_attrs);
    if (wifiTaskHandle == NULL) {
        APP_LOG("Failed to create WiFi task\n");
    } else {
        APP_LOG("wifiTask created\n");
    }

    // // MQTT任务
    // mqttTaskHandle = osThreadNew(task_mqtt, NULL, &mqttTask_attrs);
    // if (mqttTaskHandle == NULL) {
    //     APP_LOG("Failed to create MQTT task\n");
    // } else {
    //     APP_LOG("MQTT task created\n");
    // }

    APP_LOG("All tasks created\n");
}

// ===========================
// 默认任务（WOL功能）
// ===========================

void appTask(void *argument)
{
    (void)argument;

    osDelay(2000);
    APP_LOG("Default task started!\n");

    while (1) {
        err_t result = send_wol();

        if (result == ERR_OK) {
            APP_LOG("WOL sent successfully via LAN\n");
            APP_LOG("WOL task idle, waiting...\n");
            osThreadSuspend(NULL);
            while(1) { osDelay(1000); }
        } else {
            APP_LOG("WOL failed (err=%d), retrying in 2s...\n", result);
            osDelay(2000);
        }
    }
}