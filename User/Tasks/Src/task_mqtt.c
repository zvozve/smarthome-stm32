#include "task_mqtt.h"
#include "mqtt_client.h"
#include "mqtt_codec.h"
#include "hlk_rm58s_manager.h"
#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_config.h"
#include "cJSON.h"

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#include <string.h>
#include <stdlib.h>

// ===========================
// 内部状态
// ===========================
typedef struct {
    mqtt_task_state_t state;
    mqtt_client_t client;
    mqtt_transport_t transport;
    mqtt_callbacks_t callbacks;
    mqtt_task_callbacks_t task_callbacks;
    device_status_t device_status;          // ← 改用 device_status_t
    
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

// ===========================
// 传输层
// ===========================
static int mqtt_send_callback(const uint8_t *data, uint16_t len) {
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
    if (g_ctx.state < MQTT_TASK_CONNECTING) return;
    
    g_ctx.msg_recv++;
    mqtt_client_process_rx(&g_ctx.client, data, len);
}

static void mqtt_status_callback(hlk_mgr_status_t status) {
    if (status == HLK_MGR_CONNECTED) {
        if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
            mqtt_task_connect();
        }
    } else if (status == HLK_MGR_DISCONNECTED) {
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
    MQTT_LOG("Connected to broker\n");
    g_ctx.state = MQTT_TASK_CONNECTED;
    g_ctx.reconnect_retry = 0;
    g_ctx.auto_reconnect = false;
    
    if (g_ctx.task_callbacks.on_state_change) {
        g_ctx.task_callbacks.on_state_change(g_ctx.state);
    }
    
    int ret = mqtt_client_subscribe(&g_ctx.client, MQTT_TOPIC_SUBSCRIBE, MQTT_QOS_1);
    if (ret >= 0) {
        g_ctx.state = MQTT_TASK_SUBSCRIBING;
        MQTT_LOG("Subscribing to %s\n", MQTT_TOPIC_SUBSCRIBE);
    }
}

static void on_mqtt_disconnected(void) {
    MQTT_LOG("Disconnected from broker\n");
    g_ctx.state = MQTT_TASK_ERROR;
    g_ctx.auto_reconnect = true;
    g_ctx.reconnect_tick = mqtt_tick_callback();
    if (g_ctx.task_callbacks.on_state_change) {
        g_ctx.task_callbacks.on_state_change(g_ctx.state);
    }
}

static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len) {
    MQTT_LOG("Recv: %s\n", topic);
    g_ctx.msg_recv++;
    
    // 命令: smart_home/cmd/xxx
    if (strncmp(topic, "smart_home/cmd/", 15) == 0) {
        char cmd_buf[32];
        char *params = NULL;
        mqtt_parse_command(topic, payload, len, cmd_buf, sizeof(cmd_buf), &params);
        
        if (g_ctx.task_callbacks.on_command) {
            g_ctx.task_callbacks.on_command(cmd_buf, params);
        }
        if (params) free(params);
        return;
    }
    
    // 数据请求: smart_home/data/xxx
    if (strncmp(topic, "smart_home/data/", 16) == 0) {
        const char *type = topic + 16;
        if (g_ctx.task_callbacks.on_data_request) {
            g_ctx.task_callbacks.on_data_request(type);
        }
        return;
    }
    
    // 配置: smart_home/config
    if (strcmp(topic, "smart_home/config") == 0) {
        char *json_str = NULL;
        if (payload && len > 0) {
            json_str = malloc(len + 1);
            if (json_str) {
                memcpy(json_str, payload, len);
                json_str[len] = '\0';
            }
        }
        if (g_ctx.task_callbacks.on_config) {
            g_ctx.task_callbacks.on_config(json_str);
        }
        if (json_str) free(json_str);
    }
}

static void on_mqtt_subscribed(const char *topic, uint8_t result) {
    if (result == 0x80) {
        MQTT_LOG("Subscribe failed\n");
        g_ctx.state = MQTT_TASK_ERROR;
    } else {
        MQTT_LOG("Subscribe success: %s\n", topic);
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
}

// ===========================
// MQTT连接
// ===========================
static void mqtt_task_connect(void) {
    MQTT_LOG("Connecting to %s:%d\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
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
    }
}

// ===========================
// 发送状态
// ===========================
static void mqtt_task_send_status_internal(void) {
    if (!mqtt_client_is_connected(&g_ctx.client)) return;
    if (g_ctx.state != MQTT_TASK_READY) return;
    if (!g_ctx.task_callbacks.get_device_status) return;
    
    device_status_t status;          // ← 改用 device_status_t
    g_ctx.task_callbacks.get_device_status(&status);
    memcpy(&g_ctx.device_status, &status, sizeof(device_status_t));
    
    char *json = mqtt_status_to_json(&status);
    if (!json) return;
    
    mqtt_client_publish(&g_ctx.client,
                        MQTT_TOPIC_STATUS,
                        (uint8_t*)json,
                        strlen(json),
                        MQTT_QOS_1, false);
    
    free(json);
    g_ctx.last_status_tick = mqtt_tick_callback();
}

// ===========================
// 任务主循环
// ===========================
static void mqtt_task_process(void) {
    uint32_t now = mqtt_tick_callback();
    hlk_mgr_status_t wifi_status = HLK_Manager_GetStatus();
    
    if (wifi_status != HLK_MGR_CONNECTED) {
        if (g_ctx.state != MQTT_TASK_WAIT_WIFI) {
            g_ctx.state = MQTT_TASK_WAIT_WIFI;
            if (g_ctx.task_callbacks.on_state_change) {
                g_ctx.task_callbacks.on_state_change(g_ctx.state);
            }
        }
        return;
    }
    
    if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
        mqtt_task_connect();
        return;
    }
    
    if (g_ctx.auto_reconnect) {
        uint32_t delay = (g_ctx.reconnect_retry < 5) ? 5000 : 30000;
        if (now - g_ctx.reconnect_tick > delay) {
            g_ctx.reconnect_retry++;
            mqtt_task_connect();
        }
        return;
    }
    
    if (g_ctx.state >= MQTT_TASK_CONNECTING) {
        mqtt_client_loop(&g_ctx.client);
    }
    
    if (g_ctx.state == MQTT_TASK_READY) {
        if (now - g_ctx.last_status_tick > 30000) {
            mqtt_task_send_status_internal();
        }
    }
}

static void mqtt_task_routine(void *arg) {
    (void)arg;
    
    MQTT_LOG("MQTT Task Started\n");
    
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
    
    hlk_mgr_status_t current_status = HLK_Manager_GetStatus();
    if (current_status == HLK_MGR_CONNECTED) {
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        mqtt_task_connect();
        g_ctx.last_status_tick = mqtt_tick_callback();
    } else {
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        g_ctx.last_status_tick = mqtt_tick_callback();
    }
    
    while (1) {
        mqtt_task_process();
        HLK_Delay_ms(100);
    }
}

// ===========================
// 对外API
// ===========================
void MQTT_Task_Init(mqtt_task_callbacks_t *callbacks) {
    if (g_ctx.initialized) return;
    
    if (callbacks) {
        memcpy(&g_ctx.task_callbacks, callbacks, sizeof(mqtt_task_callbacks_t));
    }
    
    g_ctx.initialized = true;
    
#if HLK_USE_RTOS
    BaseType_t ret = xTaskCreate(mqtt_task_routine, 
                                 "MQTT", 
                                 MQTT_TASK_STACK_SIZE, 
                                 NULL, 
                                 MQTT_TASK_PRIORITY, 
                                 &g_ctx.task_handle);
    if (ret == pdPASS) {
        MQTT_LOG("Task created\n");
    } else {
        MQTT_LOG("Task creation FAILED!\n");
    }
#else
    mqtt_task_routine(NULL);
#endif
}

void MQTT_Task_SendStatus(void) {
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
        if (data) cJSON_AddItemToObject(root, "data", data);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (json_str) {
        mqtt_client_publish(&g_ctx.client, MQTT_TOPIC_EVENT, (uint8_t*)json_str, strlen(json_str), MQTT_QOS_1, false);
        free(json_str);
    }
}

void MQTT_Task_SendData(const char *topic_suffix, const char *data_json) {
    if (!topic_suffix || !data_json) {
        MQTT_LOG("SendData: invalid params\n");
        return;
    }
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
    
    MQTT_LOG("SendData: %s\n", full_topic);
    mqtt_client_publish(&g_ctx.client, full_topic, (uint8_t*)data_json, strlen(data_json), MQTT_QOS_1, false);
}

void MQTT_Task_Publish(const char *topic, const char *payload) {
    if (!topic || !payload) {
        MQTT_LOG("Publish: invalid params\n");
        return;
    }
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("Publish skipped: not connected\n");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("Publish skipped: state=%d\n", g_ctx.state);
        return;
    }
    
    MQTT_LOG("Publish: %s\n", topic);
    mqtt_client_publish(&g_ctx.client, topic, (uint8_t*)payload, strlen(payload), MQTT_QOS_1, false);
}

mqtt_task_state_t MQTT_Task_GetState(void) {
    return g_ctx.state;
}

bool MQTT_Task_IsReady(void) {
    return (g_ctx.state == MQTT_TASK_READY);
}