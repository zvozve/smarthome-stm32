#include "app_main.h"
#include <string.h>      // ← 添加

// Hardware
#include "lan8720a_port.h"
#include "task_uart_test.h"
#include "hlk_rm58s_manager.h"

// Protocols
#include "wol.h"
#include "task_mqtt.h"
#include "task_wol.h"

// ===========================
// 任务句柄定义
// ===========================
osThreadId_t appTaskHandle;
osThreadId_t wifiTaskHandle;
osThreadId_t wifiPollTaskHandle;
osThreadId_t mqttTaskHandle;
osThreadId_t wolTaskHandle;

// ===========================
// MQTT 业务回调
// ===========================

static void on_mqtt_command(const char *cmd, const char *params_json) {
    APP_LOG("MQTT CMD: %s, params: %s\n", cmd, params_json ? params_json : "null");
    
    if (strcmp(cmd, "wol") == 0) {
        WOL_Task_OnCommand(cmd, params_json);
        return;
    }
    
    if (strcmp(cmd, "query") == 0) {
        MQTT_Task_SendStatus();
        return;
    }
    
    APP_LOG("Unknown command: %s\n", cmd);
}

static void on_mqtt_data_request(const char *type) {
    APP_LOG("MQTT Data request: %s\n", type);
    
    if (strcmp(type, "wol_status") == 0) {
        wol_state_t state = WOL_Task_GetState();
        const char *state_str[] = {"IDLE", "SENDING", "SUCCESS", "FAILED"};
        char json[64];
        snprintf(json, sizeof(json), "{\"wol_status\":\"%s\"}", state_str[state]);
        MQTT_Task_SendData("wol", json);
        return;
    }
    
    if (strcmp(type, "all") == 0) {
        MQTT_Task_SendStatus();
        return;
    }
}

static void on_mqtt_config(const char *config_json) {
    APP_LOG("MQTT Config: %s\n", config_json);
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
    
    status->temperature = 25.5;
    status->humidity = 60.0;
    status->led_state = 0;
    status->relay_state = 0;
    status->uptime = osKernelGetTickCount() / 1000;
}

// ===========================
// 应用初始化
// ===========================

void app_main_init(void) {
    SEGGER_RTT_Init();
    APP_LOG("System initializing...\n");
    APP_LOG("FIRMWARE_VERSION %s\n", FIRMWARE_VERSION);

    LAN_HardwareReset();
    app_main_create_tasks();
}

// ===========================
// 创建所有任务
// ===========================

void app_main_create_tasks(void) {
    WOL_Task_Init();
    APP_LOG("WOL_Task initialized\n");
    
    HLK_Manager_Init();
    APP_LOG("HLK_Manager initialized\n");

    mqtt_task_callbacks_t callbacks = {
        .on_command = on_mqtt_command,
        .on_data_request = on_mqtt_data_request,
        .on_config = on_mqtt_config,
        .on_state_change = on_mqtt_state_change,
        .get_device_status = get_device_status,
    };

    MQTT_Task_Init(&callbacks);
    APP_LOG("MQTT_Task initialized\n");

    APP_LOG("All tasks created\n");
}

// ===========================
// 默认任务 (保留备用)
// ===========================

void appTask(void *argument) {
    (void)argument;
    osDelay(2000);
    APP_LOG("Default task started!\n");
    while (1) {
        osDelay(1000);
    }
}