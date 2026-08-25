#include "task_mqtt.h"
#include "mqtt_client.h"
#include "mqtt_codec.h"
#include "business_mqtt.h"
#include "task_wifi.h"
#include "cJSON.h"

#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// 连接超时：发出 CONNECT 后若这么久没收到 CONNACK，则判定失败并重试
#define MQTT_CONNECT_TIMEOUT_MS   5000

// ===========================
// 内部状态
// ===========================
typedef struct {
    mqtt_task_state_t state;
    mqtt_client_t client;
    mqtt_transport_t transport;
    mqtt_callbacks_t callbacks;

    uint32_t reconnect_tick;
    uint8_t reconnect_retry;
    bool auto_reconnect;

    TaskHandle_t task_handle;
    bool initialized;

    uint32_t msg_sent;
    uint32_t msg_recv;
    uint32_t last_status_tick;

    /* 延迟发送标志：RX 回调上下文里禁止 TX（uart_drv locked=1 时
     * uart_drv_send 返回 -3），故需要在 MQTT 任务上下文（locked=0）补发 */
    bool pending_status_send;

    /* CONNECT 发出时刻，用于 CONNACK 超时看门狗 */
    uint32_t connect_start_tick;
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

static void mqtt_rx_callback(const uint8_t *data, uint16_t len, void *ctx);
static void mqtt_status_callback(bool connected, void *ctx);

static void mqtt_on_state_change(mqtt_task_state_t state);

// 业务侧接口见 business_mqtt.h（订阅/控制解析/心跳内容由用户实现）

// ===========================
// 传输层 (经 task_wifi 干净接口，不直接依赖具体 WiFi 驱动，便于移植)
// ===========================
static int mqtt_send_callback(const uint8_t *data, uint16_t len) {
    return task_wifi_send(data, len);
}

static uint32_t mqtt_tick_callback(void) {
    return HAL_GetTick();
}

static void mqtt_delay_callback(uint32_t ms) {
    HAL_Delay(ms);
}

// ===========================
// 接收回调 (注册到 task_wifi)
// ===========================
static void mqtt_rx_callback(const uint8_t *data, uint16_t len, void *ctx) {
    (void)ctx;
    if (!data || len == 0) return;
    if (g_ctx.state < MQTT_TASK_CONNECTING) return;

    g_ctx.msg_recv++;
    mqtt_client_process_rx(&g_ctx.client, data, len);
}

static void mqtt_status_callback(bool connected, void *ctx) {
    (void)ctx;
    if (connected) {
        if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
            mqtt_task_connect();
        }
    } else {
        g_ctx.state = MQTT_TASK_WAIT_WIFI;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        mqtt_on_state_change(g_ctx.state);
    }
}

// ===========================
// MQTT协议回调
// ===========================
static void on_mqtt_connected(void) {
    MQTT_LOG("Connected to broker");
    g_ctx.state = MQTT_TASK_CONNECTED;
    g_ctx.reconnect_retry = 0;
    g_ctx.auto_reconnect = false;

    mqtt_on_state_change(g_ctx.state);
    /* 注意：本回调运行在 WiFi RX 上下文（uart_drv locked=1），
     * 此时 uart_drv_send 会返回 -3。订阅动作改由 mqtt_task_process
     * 在 MQTT 任务自己的循环里发起（locked=0），避免重入 TX。 */
}

static void on_mqtt_disconnected(void) {
    MQTT_LOG("Disconnected from broker");
    g_ctx.state = MQTT_TASK_ERROR;
    g_ctx.auto_reconnect = true;
    g_ctx.reconnect_tick = mqtt_tick_callback();
    mqtt_on_state_change(g_ctx.state);
}

static void on_mqtt_message(const char *topic, const uint8_t *payload, uint16_t len) {
    g_ctx.msg_recv++;
    MQTT_LOG("Recv: %s", topic);
    /* 消息语义（控制解析/路由）交给业务层处理，见 business_mqtt.c */
    mqtt_business_on_message(topic, payload, len);
}

static void on_mqtt_subscribed(const char *topic, uint8_t result) {
    if (result == 0x80) {
        MQTT_LOG("Subscribe failed");
        g_ctx.state = MQTT_TASK_ERROR;
    } else {
        MQTT_LOG("Subscribe success: %s", topic);
        g_ctx.state = MQTT_TASK_READY;
        g_ctx.auto_reconnect = false;
        /* 本回调同样运行在 RX 上下文中，不能在此 publish（会被 -3 拒绝）。
         * 置标志，由 mqtt_task_process 在任务上下文补发首帧状态。 */
        g_ctx.pending_status_send = true;
        mqtt_on_state_change(g_ctx.state);
    }
}

static void on_mqtt_published(uint16_t packet_id) {
    g_ctx.msg_sent++;
}

// ===========================
// 状态变更日志（通用，不属业务）
// ===========================
static void mqtt_on_state_change(mqtt_task_state_t state) {
    MQTT_LOG("MQTT state -> %d", state);
}

// ===========================
// MQTT连接
// ===========================
static void mqtt_task_connect(void) {
    /* 防重入：已在连接/已连接/订阅/就绪中，禁止再发第二个 CONNECT。
     * 否则同一 ClientID 的第二个 CONNECT 会让 broker 踢掉上一个会话，
     * 造成 EMQX 显示离线、固件却认为在线的假象。 */
    if (g_ctx.state >= MQTT_TASK_CONNECTING) return;

    MQTT_LOG("Connecting to %s:%d", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    g_ctx.state = MQTT_TASK_CONNECTING;

    /* 新的连接尝试开始：关闭自动重连闩锁（避免 process 循环里的
     * auto_reconnect 分支在 startup 遗留的旧 reconnect_tick 驱动下
     * 立即再发一次 CONNECT）*/
    g_ctx.auto_reconnect = false;
    g_ctx.reconnect_retry = 0;
    g_ctx.connect_start_tick = mqtt_tick_callback();

    mqtt_on_state_change(g_ctx.state);

    mqtt_client_set_connect_info(&g_ctx.client,
                                  MQTT_CLIENT_ID,
                                  MQTT_USERNAME,
                                  MQTT_PASSWORD);

    int ret = mqtt_client_connect(&g_ctx.client, MQTT_KEEPALIVE, true);
    if (ret != 0) {
        MQTT_LOG("Connect failed, ret=%d", ret);
        g_ctx.state = MQTT_TASK_ERROR;
        g_ctx.auto_reconnect = true;
        g_ctx.reconnect_tick = mqtt_tick_callback();
        g_ctx.reconnect_retry = 0;
    }
}

// ===========================
// 发送状态
// ===========================
static void mqtt_task_send_status_internal(void) {
    if (!mqtt_client_is_connected(&g_ctx.client)) return;
    if (g_ctx.state != MQTT_TASK_READY) return;
    device_status_t status;
    mqtt_business_get_device_status(&status);

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
    bool wifi_connected = task_wifi_is_connected();

    if (!wifi_connected) {
        if (g_ctx.state != MQTT_TASK_WAIT_WIFI) {
            g_ctx.state = MQTT_TASK_WAIT_WIFI;
            mqtt_on_state_change(g_ctx.state);
        }
        return;
    }

    if (g_ctx.state == MQTT_TASK_WAIT_WIFI || g_ctx.state == MQTT_TASK_IDLE) {
        mqtt_task_connect();
        return;
    }

    /* CONNECTING：等待 CONNACK（由 RX 回调推进到 CONNECTED）。
     * 若超时仍未收到，则转入 ERROR 触发重连，避免永远卡在 CONNECTING。 */
    if (g_ctx.state == MQTT_TASK_CONNECTING) {
        if (now - g_ctx.connect_start_tick > MQTT_CONNECT_TIMEOUT_MS) {
            MQTT_LOG("CONNACK timeout, retry");
            g_ctx.state = MQTT_TASK_ERROR;
            g_ctx.auto_reconnect = true;
            g_ctx.reconnect_tick = now;
        }
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

    /* 连接已建立但尚未订阅：在 MQTT 任务上下文（locked=0）发起订阅，
     * 不在 RX 回调里发 TX（uart_drv locked=1 时 uart_drv_send 返回 -3）。
     * 本周期处理完即返回，下一周期等 SUBACK 到达后切到 READY。 */
    if (g_ctx.state == MQTT_TASK_CONNECTED) {
        int ret = mqtt_client_subscribe(&g_ctx.client, MQTT_TOPIC_SUBSCRIBE, MQTT_QOS_1);
        if (ret >= 0) {
            g_ctx.state = MQTT_TASK_SUBSCRIBING;
            MQTT_LOG("Subscribing to %s", MQTT_TOPIC_SUBSCRIBE);
        } else {
            MQTT_LOG("Subscribe failed, ret=%d (retry next loop)", ret);
        }
        return;
    }

    if (g_ctx.state == MQTT_TASK_READY) {
        if (g_ctx.pending_status_send) {
            g_ctx.pending_status_send = false;
            mqtt_task_send_status_internal();
        } else if (now - g_ctx.last_status_tick > 30000) {
            mqtt_task_send_status_internal();
        }
    }
}

static void mqtt_task_routine(void *arg) {
    (void)arg;

    MQTT_LOG("MQTT Task Started");

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

    task_wifi_register_rx_callback(mqtt_rx_callback, NULL);
    task_wifi_register_state_callback(mqtt_status_callback, NULL);

    bool connected = task_wifi_is_connected();
    if (connected) {
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
        HAL_Delay(100);
    }
}

// ===========================
// 对外API
// ===========================
void MQTT_Task_Init(void) {
    if (g_ctx.initialized) return;

    g_ctx.initialized = true;

    BaseType_t ret = xTaskCreate(mqtt_task_routine,
                                 "MQTT",
                                 MQTT_TASK_STACK_SIZE,
                                 NULL,
                                 MQTT_TASK_PRIORITY,
                                 &g_ctx.task_handle);
    if (ret == pdPASS) {
        MQTT_LOG("Task created");
    } else {
        MQTT_LOG("Task creation FAILED!");
    }
}

void MQTT_Task_SendStatus(void) {
    mqtt_task_send_status_internal();
}

void MQTT_Task_SendEvent(const char *event, const char *data_json) {
    if (!event) return;
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("SendEvent skipped: not connected");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("SendEvent skipped: state=%d", g_ctx.state);
        return;
    }

    MQTT_LOG("SendEvent: %s", event);

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
        MQTT_LOG("SendData: invalid params");
        return;
    }
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("SendData skipped: not connected");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("SendData skipped: state=%d", g_ctx.state);
        return;
    }

    char full_topic[64];
    snprintf(full_topic, sizeof(full_topic), "%s/%s", MQTT_TOPIC_DATA, topic_suffix);

    MQTT_LOG("SendData: %s", full_topic);
    mqtt_client_publish(&g_ctx.client, full_topic, (uint8_t*)data_json, strlen(data_json), MQTT_QOS_1, false);
}

void MQTT_Task_Publish(const char *topic, const char *payload) {
    if (!topic || !payload) {
        MQTT_LOG("Publish: invalid params");
        return;
    }
    if (!mqtt_client_is_connected(&g_ctx.client)) {
        MQTT_LOG("Publish skipped: not connected");
        return;
    }
    if (g_ctx.state != MQTT_TASK_READY) {
        MQTT_LOG("Publish skipped: state=%d", g_ctx.state);
        return;
    }

    MQTT_LOG("Publish: %s", topic);
    mqtt_client_publish(&g_ctx.client, topic, (uint8_t*)payload, strlen(payload), MQTT_QOS_1, false);
}

mqtt_task_state_t MQTT_Task_GetState(void) {
    return g_ctx.state;
}

bool MQTT_Task_IsReady(void) {
    return (g_ctx.state == MQTT_TASK_READY);
}
