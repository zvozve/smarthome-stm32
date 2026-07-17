#include "task_mqtt.h"
#include "mqtt_client.h"
#include "hlk_rm58s_manager.h"
#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_config.h"
#include "cJSON.h"

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ===========================
// MQTT Task 日志 (使用 MQTT_LOG)
// ===========================
// 注意: MQTT_LOG 已在 mqtt_client.h 中定义

// ===========================
// 内部状态
// ===========================
typedef struct {
    mqtt_task_state_t state;
    mqtt_client_t client;
    mqtt_transport_t transport;
    mqtt_callbacks_t callbacks;
    mqtt_task_callbacks_t task_callbacks;
    device_status_t device_status;
    
    uint32_t reconnect_tick;
    uint8_t reconnect_retry;
    bool auto_reconnect;
    
    TaskHandle_t task_handle;
    bool initialized;
    
    uint32_t msg_sent;
    uint32_t msg_recv;
    uint32_t last_status_tick;
} mqtt_task_context_t;

static mqtt_task_context_t g_ctx = {0};

// ===========================
// 函数声明
// ===========================
static void mqtt_task_routine(void *arg);
static void mqtt_task_connect(void);
static void mqtt_task_process(void);
static void mqtt_task_send_status_internal(void);

static void on_mqtt_connected(void);
static void on_mqtt_disconnected(void);
static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len);
static void on_mqtt_subscribed(const char *topic, uint8_t result);
static void on_mqtt_published(uint16_t packet_id);

static int mqtt_send_callback(const uint8_t *data, uint16_t len);
static uint32_t mqtt_tick_callback(void);
static void mqtt_delay_callback(uint32_t ms);

static void mqtt_rx_callback(const uint8_t *data, uint16_t len);
static void mqtt_status_callback(hlk_mgr_status_t status);

static void mqtt_parse_command(const char *topic, const uint8_t *payload, uint16_t len);
static void mqtt_parse_config(const uint8_t *payload, uint16_t len);

// ===========================
// 传输层实现
// ===========================
static int mqtt_send_callback(const uint8_t *data, uint16_t len) {
    MQTT_LOG("Send: %d bytes\n", len);
    return HLK_Driver_Send(data, len);
}

static uint32_t mqtt_tick_callback(void) {
    return HLK_GET_TICK();
}

static void mqtt_delay_callback(uint32_t ms) {
    HLK_Delay_ms(ms);
}

// ===========================
// 接收回调 (注册到Manager)
// ===========================
static void mqtt_rx_callback(const uint8_t *data, uint16_t len) {
    if (!data || len == 0) return;
    if (g_ctx.state < MQTT_TASK_CONNECTING) {
        MQTT_LOG("RX ignored: state=%d\n", g_ctx.state);
        return;
    }
    
    g_ctx.msg_recv++;
    MQTT_LOG("RX callback: %d bytes, total=%d\n", len, g_ctx.msg_recv);
    mqtt_client_process_rx(&g_ctx.client, data, len);
}

static void mqtt_status_callback(hlk_mgr_status_t status) {
    MQTT_LOG("WiFi status changed: %d (%s)\n", status, 
             status == HLK_MGR_CONNECTED ? "CONNECTED" : 
             status == HLK_MGR_DISCONNECTED ? "DISCONNECTED" : "CONFIGURING");
    
    if (status == HLK_MGR_CONNECTED) {
        if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
            MQTT_LOG("WiFi connected, starting MQTT...\n");
            mqtt_task_connect();
        }
    } else if (status == HLK_MGR_DISCONNECTED) {
        MQTT_LOG("WiFi disconnected\n");
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        if (g_ctx.task_callbacks.on_state_change) {
            g_ctx.task_callbacks.on_state_change(g_ctx.state);
        }
    }
}

// ===========================
// MQTT协议回调
// ===========================
static void on_mqtt_connected(void) {
    MQTT_LOG("*** Connected to broker! ***\n");
    g_ctx.state = MQTT_TASK_CONNECTED;
    g_ctx.reconnect_retry = 0;
    g_ctx.auto_reconnect = false;
    
    if (g_ctx.task_callbacks.on_state_change) {
        g_ctx.task_callbacks.on_state_change(g_ctx.state);
    }
    
    int ret = mqtt_client_subscribe(&g_ctx.client, MQTT_TOPIC_SUBSCRIBE, MQTT_QOS_1);
    if (ret >= 0) {
        g_ctx.state = MQTT_TASK_SUBSCRIBING;
        MQTT_LOG("Subscribing to %s (id=%d)\n", MQTT_TOPIC_SUBSCRIBE, ret);
    } else {
        MQTT_LOG("Subscribe failed, ret=%d\n", ret);
    }
}

static void on_mqtt_disconnected(void) {
    MQTT_LOG("*** Disconnected from broker ***\n");
    g_ctx.state = MQTT_TASK_ERROR;
    g_ctx.auto_reconnect = true;
    g_ctx.reconnect_tick = mqtt_tick_callback();
    
    if (g_ctx.task_callbacks.on_state_change) {
        g_ctx.task_callbacks.on_state_change(g_ctx.state);
    }
}

static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len) {
    MQTT_LOG("Received: topic=%s, len=%d\n", topic, len);
    g_ctx.msg_recv++;
    
    if (strncmp(topic, "smart_home/cmd/", 15) == 0) {
        mqtt_parse_command(topic, payload, len);
    } else if (strncmp(topic, "smart_home/data/", 16) == 0) {
        const char *type = topic + 16;
        MQTT_LOG("Data request: %s\n", type);
        if (g_ctx.task_callbacks.on_data_request) {
            g_ctx.task_callbacks.on_data_request(type);
        }
    } else if (strcmp(topic, "smart_home/config") == 0) {
        MQTT_LOG("Config update\n");
        mqtt_parse_config(payload, len);
    }
}

static void on_mqtt_subscribed(const char *topic, uint8_t result) {
    if (result == 0x80) {
        MQTT_LOG("Subscribe failed!\n");
        g_ctx.state = MQTT_TASK_ERROR;
    } else {
        MQTT_LOG("Subscribe success: topic=%s, qos=%d\n", topic, result);
        g_ctx.state = MQTT_TASK_READY;
        g_ctx.auto_reconnect = false;
        
        mqtt_task_send_status_internal();
        
        if (g_ctx.task_callbacks.on_state_change) {
            g_ctx.task_callbacks.on_state_change(g_ctx.state);
        }
    }
}

static void on_mqtt_published(uint16_t packet_id) {
    g_ctx.msg_sent++;
    MQTT_LOG("Published: id=%d (total=%d)\n", packet_id, g_ctx.msg_sent);
}

// ===========================
// MQTT连接
// ===========================
static void mqtt_task_connect(void) {
    MQTT_LOG("=== Connecting to %s:%d ===\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    g_ctx.state = MQTT_TASK_CONNECTING;
    
    if (g_ctx.task_callbacks.on_state_change) {
        g_ctx.task_callbacks.on_state_change(g_ctx.state);
    }
    
    mqtt_client_set_connect_info(&g_ctx.client, 
                                  MQTT_CLIENT_ID,
                                  MQTT_USERNAME, 
                                  MQTT_PASSWORD);
    
    int ret = mqtt_client_connect(&g_ctx.client, MQTT_KEEPALIVE, true);
    if (ret != 0) {
        MQTT_LOG("Connect failed, ret=%d\n", ret);
        g_ctx.state = MQTT_TASK_ERROR;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        g_ctx.reconnect_retry++;
    } else {
        MQTT_LOG("Connect packet sent, waiting for CONNACK...\n");
    }
}

// ===========================
// 消息解析
// ===========================
static void mqtt_parse_command(const char *topic, const uint8_t *payload, uint16_t len) {
    const char *cmd = topic + 15;
    MQTT_LOG("Command: %s\n", cmd);
    
    if (payload && len > 0) {
        char *json_str = (char*)malloc(len + 1);
        if (json_str) {
            memcpy(json_str, payload, len);
            json_str[len] = '\0';
            MQTT_LOG("Command params: %s\n", json_str);
            
            if (g_ctx.task_callbacks.on_command) {
                g_ctx.task_callbacks.on_command(cmd, json_str);
            }
            free(json_str);
        }
    } else {
        MQTT_LOG("Command no params\n");
        if (g_ctx.task_callbacks.on_command) {
            g_ctx.task_callbacks.on_command(cmd, NULL);
        }
    }
}

static void mqtt_parse_config(const uint8_t *payload, uint16_t len) {
    if (!payload || len == 0) return;
    
    char *json_str = (char*)malloc(len + 1);
    if (!json_str) return;
    
    memcpy(json_str, payload, len);
    json_str[len] = '\0';
    MQTT_LOG("Config: %s\n", json_str);
    
    if (g_ctx.task_callbacks.on_config) {
        g_ctx.task_callbacks.on_config(json_str);
    }
    free(json_str);
}

// ===========================
// 发送状态 (内部)
// ===========================
static void mqtt_task_send_status_internal(void) {
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("Send status skipped: not connected\n");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("Send status skipped: state=%d\n", g_ctx.state);
        return;
    }
    if (!g_ctx.task_callbacks.get_device_status) {
        MQTT_LOG("Send status skipped: no callback\n");
        return;
    }
    
    device_status_t status;
    g_ctx.task_callbacks.get_device_status(&status);
    memcpy(&g_ctx.device_status, &status, sizeof(device_status_t));
    
    char *json = MQTT_StatusToJson(&status);
    if (!json) return;
    
    MQTT_LOG("Send status: %s\n", json);
    
    mqtt_client_publish(&g_ctx.client,
                        MQTT_TOPIC_STATUS,
                        (uint8_t*)json,
                        strlen(json),
                        MQTT_QOS_1, false);
    
    free(json);
    g_ctx.last_status_tick = mqtt_tick_callback();
}

// ===========================
// MQTT任务主循环
// ===========================
static void mqtt_task_process(void) {
    uint32_t now = mqtt_tick_callback();
    
    hlk_mgr_status_t wifi_status = HLK_Manager_GetStatus();
    
    // WiFi未连接
    if (wifi_status != HLK_MGR_CONNECTED) {
        if (g_ctx.state != MQTT_TASK_WAIT_WIFI) {
            g_ctx.state = MQTT_TASK_WAIT_WIFI;
            MQTT_LOG("Waiting for WiFi (status=%d)\n", wifi_status);
            if (g_ctx.task_callbacks.on_state_change) {
                g_ctx.task_callbacks.on_state_change(g_ctx.state);
            }
        }
        return;
    }
    
    // WiFi已连接，但MQTT还没开始连接
    if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
        MQTT_LOG("WiFi connected, starting MQTT...\n");
        mqtt_task_connect();
        return;
    }
    
    // 处理重连
    if (g_ctx.auto_reconnect) {
        uint32_t delay = (g_ctx.reconnect_retry < 5) ? 5000 : 30000;
        if (now - g_ctx.reconnect_tick > delay) {
            g_ctx.reconnect_retry++;
            MQTT_LOG("Reconnect attempt %d\n", g_ctx.reconnect_retry);
            mqtt_task_connect();
        }
        return;
    }
    
    // 运行MQTT客户端心跳
    if (g_ctx.state >= MQTT_TASK_CONNECTING) {
        mqtt_client_loop(&g_ctx.client);
    }
    
    // 定期发送状态 (30秒)
    if (g_ctx.state == MQTT_TASK_READY) {
        if (now - g_ctx.last_status_tick > 30000) {
            MQTT_LOG("Periodic status update\n");
            mqtt_task_send_status_internal();
        }
    }
}

static void mqtt_task_routine(void *arg) {
    (void)arg;
    
    MQTT_LOG("=== MQTT Task Started ===\n");
    
    g_ctx.transport.send = mqtt_send_callback;
    g_ctx.transport.get_tick = mqtt_tick_callback;
    g_ctx.transport.delay = mqtt_delay_callback;
    mqtt_client_init(&g_ctx.client, &g_ctx.transport);
    
    g_ctx.callbacks.on_connect = on_mqtt_connected;
    g_ctx.callbacks.on_disconnect = on_mqtt_disconnected;
    g_ctx.callbacks.on_message = on_mqtt_message;
    g_ctx.callbacks.on_subscribe = on_mqtt_subscribed;
    g_ctx.callbacks.on_publish = on_mqtt_published;
    g_ctx.callbacks.on_unsubscribe = NULL;
    mqtt_client_set_callbacks(&g_ctx.client, &g_ctx.callbacks);
    
    HLK_Manager_RegisterRxCallback(mqtt_rx_callback);
    HLK_Manager_RegisterStatusCallback(mqtt_status_callback);
    
    // 立即检查当前WiFi状态
    hlk_mgr_status_t current_status = HLK_Manager_GetStatus();
    MQTT_LOG("Current WiFi status: %d\n", current_status);
    
    if (current_status == HLK_MGR_CONNECTED) {
        MQTT_LOG("WiFi already connected, starting MQTT immediately...\n");
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        mqtt_task_connect();
        g_ctx.last_status_tick = mqtt_tick_callback();
    } else {
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        g_ctx.last_status_tick = mqtt_tick_callback();
        MQTT_LOG("Waiting for WiFi...\n");
    }
    
    MQTT_LOG("Enter main loop\n");
    
    while (1) {
        mqtt_task_process();
        HLK_Delay_ms(100);
    }
}

// ===========================
// JSON辅助函数 (保持不变)
// ===========================
char* MQTT_StatusToJson(const device_status_t *status) {
    if (!status) return NULL;
    
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddNumberToObject(root, "temperature", status->temperature);
    cJSON_AddNumberToObject(root, "humidity", status->humidity);
    cJSON_AddNumberToObject(root, "led", status->led_state);
    cJSON_AddNumberToObject(root, "relay", status->relay_state);
    cJSON_AddNumberToObject(root, "uptime", status->uptime);
    cJSON_AddStringToObject(root, "status", "online");
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

// ===========================
// 对外API
// ===========================
void MQTT_Task_Init(mqtt_task_callbacks_t *callbacks) {
    if (g_ctx.initialized) return;
    
    MQTT_LOG("Initializing...\n");
    
    if (callbacks) {
        memcpy(&g_ctx.task_callbacks, callbacks, sizeof(mqtt_task_callbacks_t));
        MQTT_LOG("Callbacks registered\n");
    }
    
    g_ctx.initialized = true;
    
#if HLK_USE_RTOS
    // 检查 FreeRTOS 堆剩余
    MQTT_LOG("Free heap: %d bytes\n", xPortGetFreeHeapSize());
    
    BaseType_t ret = xTaskCreate(mqtt_task_routine, 
                                 "MQTT",              // 缩短任务名
                                 MQTT_TASK_STACK_SIZE, 
                                 NULL, 
                                 MQTT_TASK_PRIORITY, 
                                 &g_ctx.task_handle);
    if (ret == pdPASS) {
        MQTT_LOG("Task created successfully\n");
    } else {
        MQTT_LOG("Task creation FAILED! ret=%d\n", ret);
        MQTT_LOG("Try reducing stack size\n");
    }
#else
    mqtt_task_routine(NULL);
#endif
}

void MQTT_Task_SendStatus(void) {
    MQTT_LOG("SendStatus called\n");
    mqtt_task_send_status_internal();
}

void MQTT_Task_SendEvent(const char *event, const char *data_json) {
    if (!event) return;
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("SendEvent skipped: not connected\n");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("SendEvent skipped: state=%d\n", g_ctx.state);
        return;
    }
    
    MQTT_LOG("SendEvent: %s\n", event);
    
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    
    cJSON_AddStringToObject(root, "event", event);
    cJSON_AddNumberToObject(root, "timestamp", mqtt_tick_callback() / 1000);
    
    if (data_json) {
        cJSON *data = cJSON_Parse(data_json);
        if (data) {
            cJSON_AddItemToObject(root, "data", data);
        }
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str) {
        mqtt_client_publish(&g_ctx.client,
                            MQTT_TOPIC_EVENT,
                            (uint8_t*)json_str,
                            strlen(json_str),
                            MQTT_QOS_1, false);
        free(json_str);
    }
}

void MQTT_Task_SendData(const char *topic_suffix, const char *data_json) {
    if (!topic_suffix || !data_json) return;
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("SendData skipped: not connected\n");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("SendData skipped: state=%d\n", g_ctx.state);
        return;
    }
    
    char full_topic[64];
    snprintf(full_topic, sizeof(full_topic), "%s/%s", MQTT_TOPIC_DATA, topic_suffix);
    
    MQTT_LOG("SendData: topic=%s\n", full_topic);
    
    mqtt_client_publish(&g_ctx.client,
                        full_topic,
                        (uint8_t*)data_json,
                        strlen(data_json),
                        MQTT_QOS_1, false);
}

void MQTT_Task_Publish(const char *topic, const char *payload) {
    if (!topic || !payload) return;
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("Publish skipped: not connected\n");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("Publish skipped: state=%d\n", g_ctx.state);
        return;
    }
    
    MQTT_LOG("Publish: topic=%s\n", topic);
    
    mqtt_client_publish(&g_ctx.client,
                        topic,
                        (uint8_t*)payload,
                        strlen(payload),
                        MQTT_QOS_1, false);
}

mqtt_task_state_t MQTT_Task_GetState(void) {
    return g_ctx.state;
}

bool MQTT_Task_IsReady(void) {
    return (g_ctx.state == MQTT_TASK_READY);
}