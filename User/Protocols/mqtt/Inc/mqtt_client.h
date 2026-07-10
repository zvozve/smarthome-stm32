#ifndef __MQTT_CLIENT_H
#define __MQTT_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "mqtt_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// MQTT 消息结构
// ===========================

typedef struct {
    char *topic;
    uint8_t *payload;
    uint16_t payload_len;
    uint8_t qos;
    bool retained;
} mqtt_message_t;

// ===========================
// MQTT 客户端配置
// ===========================

typedef struct {
    const char *client_id;
    const char *username;
    const char *password;
    uint16_t keep_alive;      // 心跳间隔（秒）
    uint8_t clean_session;    // 是否清理会话
} mqtt_config_t;

// ===========================
// 回调函数类型
// ===========================

typedef void (*mqtt_publish_cb_t)(const char *topic, uint8_t *payload, uint16_t len);
typedef void (*mqtt_connect_cb_t)(bool success);

// ===========================
// MQTT 客户端
// ===========================

typedef struct {
    mqtt_transport_t *transport;
    mqtt_config_t config;

    bool connected;
    uint16_t packet_id;

    mqtt_publish_cb_t publish_cb;
    mqtt_connect_cb_t connect_cb;

    uint8_t rx_buffer[1024];
    uint16_t rx_len;
} mqtt_client_t;

// ===========================
// API
// ===========================

void mqtt_client_init(mqtt_client_t *client, mqtt_transport_t *transport);
int mqtt_client_connect(mqtt_client_t *client, const char *host, uint16_t port, mqtt_config_t *config);
void mqtt_client_disconnect(mqtt_client_t *client);
bool mqtt_client_is_connected(mqtt_client_t *client);

int mqtt_client_publish(mqtt_client_t *client, const char *topic, const char *payload);
int mqtt_client_publish_bin(mqtt_client_t *client, const char *topic, uint8_t *payload, uint16_t len);
int mqtt_client_subscribe(mqtt_client_t *client, const char *topic, uint8_t qos);
int mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic);

void mqtt_client_loop(mqtt_client_t *client);

void mqtt_client_set_publish_cb(mqtt_client_t *client, mqtt_publish_cb_t cb);
void mqtt_client_set_connect_cb(mqtt_client_t *client, mqtt_connect_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_CLIENT_H */