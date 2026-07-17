#ifndef __TASK_MQTT_H__
#define __TASK_MQTT_H__

#include <stdint.h>
#include <stdbool.h>
#include "network_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// MQTT 任务配置 (引用 network_config.h)
// ===========================
#define MQTT_TASK_STACK_SIZE        2048
#define MQTT_TASK_PRIORITY          2

// MQTT 服务器配置 (从 network_config.h 引用)
// #define MQTT_BROKER_IP              MQTT_BROKER_IP
// #define MQTT_BROKER_PORT            MQTT_BROKER_PORT
// #define MQTT_CLIENT_ID              MQTT_CLIENT_ID
// #define MQTT_USERNAME               MQTT_USERNAME
// #define MQTT_PASSWORD               MQTT_PASSWORD
#define MQTT_KEEPALIVE              60

// 主题定义 (保留，不属于network_config)
#define MQTT_TOPIC_SUBSCRIBE        "smart_home/cmd/#"
#define MQTT_TOPIC_STATUS           "smart_home/status"
#define MQTT_TOPIC_DATA             "smart_home/data"
#define MQTT_TOPIC_EVENT            "smart_home/event"
#define MQTT_TOPIC_CONFIG           "smart_home/config"

// ===========================
// MQTT 任务状态
// ===========================
typedef enum {
    MQTT_TASK_IDLE = 0,
    MQTT_TASK_WAIT_WIFI,
    MQTT_TASK_CONNECTING,
    MQTT_TASK_CONNECTED,
    MQTT_TASK_SUBSCRIBING,
    MQTT_TASK_READY,
    MQTT_TASK_ERROR,
} mqtt_task_state_t;

// ===========================
// 业务数据类型
// ===========================
typedef struct {
    float temperature;
    float humidity;
    uint8_t led_state;
    uint8_t relay_state;
    uint32_t uptime;
} device_status_t;

typedef struct {
    char cmd[32];
    char params[128];
} mqtt_command_t;

// ===========================
// 业务回调 (由应用层实现)
// ===========================
typedef struct {
    // 收到控制命令: smart_home/cmd/xxx
    void (*on_command)(const char *cmd, const char *params_json);
    
    // 收到数据请求: smart_home/data/xxx
    void (*on_data_request)(const char *type);
    
    // 收到配置更新: smart_home/config
    void (*on_config)(const char *config_json);
    
    // 状态变化通知
    void (*on_state_change)(mqtt_task_state_t state);
    
    // 获取设备状态 (由应用提供)
    void (*get_device_status)(device_status_t *status);
} mqtt_task_callbacks_t;

// ===========================
// API
// ===========================
void MQTT_Task_Init(mqtt_task_callbacks_t *callbacks);
void MQTT_Task_SendStatus(void);
void MQTT_Task_SendEvent(const char *event, const char *data_json);
void MQTT_Task_SendData(const char *topic_suffix, const char *data_json);
void MQTT_Task_Publish(const char *topic, const char *payload);
mqtt_task_state_t MQTT_Task_GetState(void);
bool MQTT_Task_IsReady(void);

// 数据辅助函数
char* MQTT_StatusToJson(const device_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_MQTT_H__ */