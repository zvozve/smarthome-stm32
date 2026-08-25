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
#define MQTT_TASK_STACK_SIZE        2048
#define MQTT_TASK_PRIORITY          2

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
    MQTT_TASK_IDLE = 0,
    MQTT_TASK_WAIT_WIFI,
    MQTT_TASK_CONNECTING,
    MQTT_TASK_CONNECTED,
    MQTT_TASK_SUBSCRIBING,
    MQTT_TASK_READY,
    MQTT_TASK_ERROR,
} mqtt_task_state_t;

// ===========================
// API
// ===========================
void MQTT_Task_Init(void);
void MQTT_Task_SendStatus(void);
void MQTT_Task_SendEvent(const char *event, const char *data_json);
void MQTT_Task_SendData(const char *topic_suffix, const char *data_json);
void MQTT_Task_Publish(const char *topic, const char *payload);
mqtt_task_state_t MQTT_Task_GetState(void);
bool MQTT_Task_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_MQTT_H__ */