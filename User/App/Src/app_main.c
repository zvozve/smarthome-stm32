#include "app_main.h"

// Hardware
#include "lan8720a_port.h"
#include "task_uart_test.h"
#include "hlk_rm58s_manager.h"

// Protocols
#include "wol.h"
#include "task_mqtt.h"      // ← 添加 MQTT 任务头文件

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

// // 默认任务 (WOL)
// static const osThreadAttr_t appTask_attrs = {
//     .name = "appTask",
//     .stack_size = 256 * 4,
//     .priority = (osPriority_t) osPriorityNormal,
// };

// WiFi 管理任务 (HLK_Manager 内部已有，不需要额外创建)
// 但可以创建状态监控任务

// // MQTT 任务属性 (如果使用 osThreadNew 方式)
// static const osThreadAttr_t mqttTask_attrs = {
//     .name = "mqttTask",
//     .stack_size = 4096,
//     .priority = (osPriority_t) osPriorityNormal,
// };

// ===========================
// 外部函数声明
// ===========================

// MQTT任务主函数 (如果使用 osThreadNew 方式)
// 注意: 如果 MQTT_Task_Init 内部已经创建了任务，这里不需要再创建
// extern void mqtt_task_routine(void *arg);

// 如果需要使用 osThreadNew 方式，需要将 mqtt_task_routine 改为可外部调用

// ===========================
// 函数声明
// ===========================

void appTask(void *argument);
// void mqtt_task_entry(void *argument);  // 如果用 osThreadNew 方式

// ===========================
// MQTT 业务回调
// ===========================

static void on_mqtt_command(const char *cmd, const char *params_json) {
    APP_LOG("MQTT CMD: %s, params: %s\n", cmd, params_json ? params_json : "null");
    
    // TODO: 添加命令处理逻辑
    // 例如: led_on, led_off, relay_on, relay_off, query 等
    
    // 示例: 收到命令后回复状态
    // if (strcmp(cmd, "query") == 0) {
    //     MQTT_Task_SendStatus();
    // }
}

static void on_mqtt_data_request(const char *type) {
    APP_LOG("MQTT Data request: %s\n", type);
    
    // TODO: 添加数据请求处理逻辑
    // 例如: sensor, all 等
}

static void on_mqtt_config(const char *config_json) {
    APP_LOG("MQTT Config: %s\n", config_json);
    
    // TODO: 添加配置处理逻辑
}

static void on_mqtt_state_change(mqtt_task_state_t state) {
    const char *state_str[] = {
        "IDLE", "WAIT_WIFI", "CONNECTING", 
        "CONNECTED", "SUBSCRIBING", "READY", "ERROR"
    };
    APP_LOG("MQTT State: %s\n", state_str[state]);
}

static void get_device_status(device_status_t *status) {
    if (!status) return;
    
    // TODO: 从实际硬件读取状态
    status->temperature = 25.5;
    status->humidity = 60.0;
    status->led_state = 0;
    status->relay_state = 0;
    status->uptime = osKernelGetTickCount() / 1000;
}

// ===========================
// 硬件初始化
// ===========================

// ===========================
// 应用初始化
// ===========================

void app_main_init(void) {
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

void app_main_create_tasks(void) {
    // 1. 创建默认任务 (WOL)
    // appTaskHandle = osThreadNew(appTask, NULL, &appTask_attrs);
    // if (appTaskHandle == NULL) {
    //     APP_LOG("Failed to create appTask\n");
    // } else {
    //     APP_LOG("appTask created\n");
    // }

    // 2. 初始化 HLK WiFi 模块 (内部会创建 WiFiMgr 任务)
    HLK_Manager_Init();
    APP_LOG("HLK_Manager initialized\n");

    // 3. 注册 MQTT 业务回调
    mqtt_task_callbacks_t callbacks = {
        .on_command = on_mqtt_command,
        .on_data_request = on_mqtt_data_request,
        .on_config = on_mqtt_config,
        .on_state_change = on_mqtt_state_change,
        .get_device_status = get_device_status,
    };

    // 4. 初始化 MQTT 任务 (内部会创建 MQTTTask)
    MQTT_Task_Init(&callbacks);
    APP_LOG("MQTT_Task initialized\n");

    APP_LOG("All tasks created\n");
}

// ===========================
// 默认任务 (WOL功能)
// ===========================

void appTask(void *argument) {
    (void)argument;

    osDelay(2000);
    APP_LOG("Default task started!\n");

    while (1) {
        err_t result = send_wol();

        if (result == ERR_OK) {
            APP_LOG("WOL sent successfully via LAN\n");
            APP_LOG("WOL task idle, waiting...\n");
            osThreadSuspend(NULL);
            while (1) { osDelay(1000); }
        } else {
            APP_LOG("WOL failed (err=%d), retrying in 2s...\n", result);
            osDelay(2000);
        }
    }
}