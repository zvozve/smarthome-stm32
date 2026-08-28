#ifndef __TASK_MQTT_H__
#define __TASK_MQTT_H__

#include <stdint.h>
#include <stdbool.h>
#include "config_network.h"
#include "mqtt_codec.h" 

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// MQTT 任务配置 (通用运行时参数)
// ===========================
#define TASK_MQTT_STACK_SIZE        2048
#define TASK_MQTT_PRIORITY          2

// MQTT 服务器配置 (从 config_network.h 引用)
// #define MQTT_BROKER_IP              MQTT_BROKER_IP
// #define MQTT_BROKER_PORT            MQTT_BROKER_PORT
// #define MQTT_CLIENT_ID              MQTT_CLIENT_ID
// #define MQTT_USERNAME               MQTT_USERNAME
// #define MQTT_PASSWORD               MQTT_PASSWORD
#define MQTT_KEEPALIVE              60

// 主题定义见 business_mqtt.h（业务拥有：订阅什么 / 发布到哪）

// ===========================
// MQTT 任务状态
// ===========================
typedef enum {
    TASK_MQTT_IDLE = 0,
    TASK_MQTT_WAIT_WIFI,
    TASK_MQTT_CONNECTING,
    TASK_MQTT_CONNECTED,
    TASK_MQTT_SUBSCRIBING,
    TASK_MQTT_READY,
    TASK_MQTT_ERROR,
} task_mqtt_state_t;

// ===========================
// API
// ===========================
void Task_MQTT_Init(void);
void Task_MQTT_SendStatus(void);
void Task_MQTT_SendEvent(const char *event, const char *data_json);
void Task_MQTT_SendData(const char *topic_suffix, const char *data_json);
void Task_MQTT_Publish(const char *topic, const char *payload);
task_mqtt_state_t Task_MQTT_GetState(void);
bool Task_MQTT_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_MQTT_H__ */