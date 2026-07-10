#include "mqtt_client.h"
#include <string.h>
#include <stdlib.h>
#include "SEGGER_RTT.h"

#define MQTT_LOG(fmt, ...) SEGGER_RTT_printf(0, "[MQTT] " fmt, ##__VA_ARGS__)

// ===========================
// MQTT 固定头部类型
// ===========================

#define MQTT_MSG_CONNECT     0x10
#define MQTT_MSG_CONNACK     0x20
#define MQTT_MSG_PUBLISH     0x30
#define MQTT_MSG_PUBACK      0x40
#define MQTT_MSG_SUBSCRIBE   0x82
#define MQTT_MSG_SUBACK      0x90
#define MQTT_MSG_UNSUBSCRIBE 0xA2
#define MQTT_MSG_UNSUBACK    0xB0
#define MQTT_MSG_PINGREQ     0xC0
#define MQTT_MSG_PINGRESP    0xD0
#define MQTT_MSG_DISCONNECT  0xE0

// ===========================
// 内部函数：编码剩余长度
// ===========================

static int mqtt_encode_remain_len(uint8_t *buf, uint32_t len) {
    int pos = 0;
    do {
        uint8_t byte = len & 0x7F;
        len >>= 7;
        if (len > 0) byte |= 0x80;
        buf[pos++] = byte;
    } while (len > 0);
    return pos;
}

// ===========================
// 内部函数：解码剩余长度
// ===========================

static uint32_t mqtt_decode_remain_len(const uint8_t *buf, int *pos) {
    uint32_t len = 0;
    int shift = 0;
    while (1) {
        uint8_t byte = buf[(*pos)++];
        len |= (byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    return len;
}

// ===========================
// 构建 CONNECT 包
// ===========================

static int mqtt_build_connect(mqtt_client_t *client, uint8_t *buf) {
    int pos = 0;
    uint16_t id_len = strlen(client->config.client_id);
    uint16_t user_len = client->config.username ? strlen(client->config.username) : 0;
    uint16_t pass_len = client->config.password ? strlen(client->config.password) : 0;

    // 固定头部
    buf[pos++] = MQTT_MSG_CONNECT;
    int remain_start = pos;
    pos += 2;  // 剩余长度占位

    // 协议名: "MQTT"
    buf[pos++] = 0x00;
    buf[pos++] = 0x04;
    memcpy(&buf[pos], "MQTT", 4);
    pos += 4;

    // 协议级别: 4 (MQTT 3.1.1)
    buf[pos++] = 0x04;

    // 连接标志
    uint8_t flags = 0;
    if (client->config.username) flags |= 0x80;
    if (client->config.password) flags |= 0x40;
    if (client->config.clean_session) flags |= 0x02;
    buf[pos++] = flags;

    // Keep Alive (高字节, 低字节)
    buf[pos++] = (client->config.keep_alive >> 8) & 0xFF;
    buf[pos++] = client->config.keep_alive & 0xFF;

    // Client ID
    buf[pos++] = (id_len >> 8) & 0xFF;
    buf[pos++] = id_len & 0xFF;
    memcpy(&buf[pos], client->config.client_id, id_len);
    pos += id_len;

    // Username
    if (client->config.username) {
        buf[pos++] = (user_len >> 8) & 0xFF;
        buf[pos++] = user_len & 0xFF;
        memcpy(&buf[pos], client->config.username, user_len);
        pos += user_len;
    }

    // Password
    if (client->config.password) {
        buf[pos++] = (pass_len >> 8) & 0xFF;
        buf[pos++] = pass_len & 0xFF;
        memcpy(&buf[pos], client->config.password, pass_len);
        pos += pass_len;
    }

    // 计算剩余长度
    uint32_t remain_len = pos - remain_start - 2;
    uint8_t len_buf[4];
    int len_pos = mqtt_encode_remain_len(len_buf, remain_len);
    memcpy(&buf[remain_start], len_buf, len_pos);

    return pos;
}

// ===========================
// 构建 PUBLISH 包
// ===========================

static int mqtt_build_publish(const char *topic, uint8_t *payload, uint16_t payload_len,
                               uint8_t qos, bool retained, uint16_t packet_id, uint8_t *buf) {
    int pos = 0;
    uint16_t topic_len = strlen(topic);

    // 固定头部
    uint8_t header = MQTT_MSG_PUBLISH;
    if (qos == 1) header |= 0x02;
    else if (qos == 2) header |= 0x04;
    if (retained) header |= 0x01;
    buf[pos++] = header;

    int remain_start = pos;
    pos += 4;  // 剩余长度最大4字节

    // Topic
    buf[pos++] = (topic_len >> 8) & 0xFF;
    buf[pos++] = topic_len & 0xFF;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;

    // Packet ID (QoS > 0)
    if (qos > 0) {
        buf[pos++] = (packet_id >> 8) & 0xFF;
        buf[pos++] = packet_id & 0xFF;
    }

    // Payload
    memcpy(&buf[pos], payload, payload_len);
    pos += payload_len;

    // 计算剩余长度
    uint32_t remain_len = pos - remain_start - 4;
    uint8_t len_buf[4];
    int len_pos = mqtt_encode_remain_len(len_buf, remain_len);
    memcpy(&buf[remain_start], len_buf, len_pos);

    return pos;
}

// ===========================
// 构建 SUBSCRIBE 包
// ===========================

static int mqtt_build_subscribe(const char *topic, uint8_t qos, uint16_t packet_id, uint8_t *buf) {
    int pos = 0;
    uint16_t topic_len = strlen(topic);

    // 固定头部
    buf[pos++] = MQTT_MSG_SUBSCRIBE | (qos > 0 ? 0x02 : 0x00);

    int remain_start = pos;
    pos += 4;

    // Packet ID
    buf[pos++] = (packet_id >> 8) & 0xFF;
    buf[pos++] = packet_id & 0xFF;

    // Topic
    buf[pos++] = (topic_len >> 8) & 0xFF;
    buf[pos++] = topic_len & 0xFF;
    memcpy(&buf[pos], topic, topic_len);
    pos += topic_len;

    // QoS
    buf[pos++] = qos;

    // 计算剩余长度
    uint32_t remain_len = pos - remain_start - 4;
    uint8_t len_buf[4];
    int len_pos = mqtt_encode_remain_len(len_buf, remain_len);
    memcpy(&buf[remain_start], len_buf, len_pos);

    return pos;
}

// ===========================
// 处理接收消息
// ===========================

// mqtt_client.c 中 mqtt_handle_publish 函数修复
static void mqtt_handle_publish(mqtt_client_t *client, uint8_t *data, uint16_t len) {
    int pos = 0;
    uint8_t header = data[pos++];
    uint32_t remain_len = mqtt_decode_remain_len(data, &pos);

    // 修复：分开赋值，避免未定义行为
    uint16_t topic_len = (data[pos] << 8) | data[pos + 1];
    pos += 2;

    char topic[128];
    memcpy(topic, &data[pos], topic_len);
    topic[topic_len] = '\0';
    pos += topic_len;

    uint8_t qos = (header >> 1) & 0x03;
    if (qos > 0) {
        pos += 2;
    }

    uint16_t payload_len = remain_len - (topic_len + 2 + (qos > 0 ? 2 : 0));
    uint8_t *payload = &data[pos];

    if (client->publish_cb) {
        client->publish_cb(topic, payload, payload_len);
    }
}

// ===========================
// 公共 API
// ===========================

void mqtt_client_init(mqtt_client_t *client, mqtt_transport_t *transport) {
    memset(client, 0, sizeof(mqtt_client_t));
    client->transport = transport;
    client->packet_id = 1;
    MQTT_LOG("Init\n");
}

int mqtt_client_connect(mqtt_client_t *client, const char *host, uint16_t port, mqtt_config_t *config) {
    if (!client || !client->transport) return -1;

    MQTT_LOG("Connecting to %s:%d\n", host, port);

    // 保存配置
    client->config = *config;

    // 1. 初始化传输层
    if (client->transport->init(NULL) != 0) {
        MQTT_LOG("Transport init failed\n");
        return -1;
    }

    // 2. TCP连接
    if (client->transport->connect(host, port) != 0) {
        MQTT_LOG("TCP connect failed\n");
        return -1;
    }

    // 3. 构建 CONNECT 包
    uint8_t connect_pkt[256];
    int pkt_len = mqtt_build_connect(client, connect_pkt);

    // 4. 发送
    if (client->transport->send(connect_pkt, pkt_len) != pkt_len) {
        MQTT_LOG("Send CONNECT failed\n");
        return -1;
    }

    // 5. 等待 CONNACK
    uint8_t connack[8];
    int recv_len = client->transport->recv(connack, sizeof(connack), 2000);
    if (recv_len < 4) {
        MQTT_LOG("CONNACK timeout\n");
        return -1;
    }

    // 检查返回码
    if (connack[3] != 0) {
        MQTT_LOG("CONNACK failed, code=%d\n", connack[3]);
        return -1;
    }

    client->connected = true;
    MQTT_LOG("Connected!\n");

    if (client->connect_cb) {
        client->connect_cb(true);
    }

    return 0;
}

void mqtt_client_disconnect(mqtt_client_t *client) {
    if (!client || !client->connected) return;

    uint8_t disconnect_pkt[2] = {MQTT_MSG_DISCONNECT, 0x00};
    client->transport->send(disconnect_pkt, 2);
    client->transport->disconnect();
    client->connected = false;

    MQTT_LOG("Disconnected\n");
}

bool mqtt_client_is_connected(mqtt_client_t *client) {
    return client && client->connected && client->transport->is_connected();
}

int mqtt_client_publish(mqtt_client_t *client, const char *topic, const char *payload) {
    return mqtt_client_publish_bin(client, topic, (uint8_t*)payload, strlen(payload));
}

int mqtt_client_publish_bin(mqtt_client_t *client, const char *topic, uint8_t *payload, uint16_t len) {
    if (!client || !client->connected) return -1;

    uint8_t pub_pkt[512];
    uint16_t pkt_id = client->packet_id++;

    int pkt_len = mqtt_build_publish(topic, payload, len, 0, false, pkt_id, pub_pkt);

    if (client->transport->send(pub_pkt, pkt_len) != pkt_len) {
        MQTT_LOG("Publish failed\n");
        return -1;
    }

    MQTT_LOG("Published: topic=%s, len=%d\n", topic, len);
    return 0;
}

int mqtt_client_subscribe(mqtt_client_t *client, const char *topic, uint8_t qos) {
    if (!client || !client->connected) return -1;

    uint8_t sub_pkt[256];
    uint16_t pkt_id = client->packet_id++;

    int pkt_len = mqtt_build_subscribe(topic, qos, pkt_id, sub_pkt);

    if (client->transport->send(sub_pkt, pkt_len) != pkt_len) {
        MQTT_LOG("Subscribe failed\n");
        return -1;
    }

    MQTT_LOG("Subscribed: %s\n", topic);
    return 0;
}

int mqtt_client_unsubscribe(mqtt_client_t *client, const char *topic) {
    // 简化实现
    (void)client;
    (void)topic;
    return 0;
}

void mqtt_client_loop(mqtt_client_t *client) {
    if (!client || !client->connected) return;

    uint8_t header[2];
    int len = client->transport->recv(header, 2, 10);  // 非阻塞
    if (len < 2) return;

    uint8_t type = header[0] & 0xF0;

    // 读取完整消息
    int pos = 1;
    uint32_t remain_len = mqtt_decode_remain_len(header, &pos);

    uint8_t msg[512];
    if (client->transport->recv(msg, remain_len, 100) < remain_len) {
        return;
    }

    // 合并头部和消息
    uint8_t full_msg[512];
    memcpy(full_msg, header, pos);
    memcpy(full_msg + pos, msg, remain_len);
    uint16_t full_len = pos + remain_len;

    // 处理消息
    switch (type) {
        case MQTT_MSG_PUBLISH:
            mqtt_handle_publish(client, full_msg, full_len);
            break;

        case MQTT_MSG_PINGRESP:
            MQTT_LOG("PINGRESP\n");
            break;

        default:
            break;
    }
}

void mqtt_client_set_publish_cb(mqtt_client_t *client, mqtt_publish_cb_t cb) {
    if (client) client->publish_cb = cb;
}

void mqtt_client_set_connect_cb(mqtt_client_t *client, mqtt_connect_cb_t cb) {
    if (client) client->connect_cb = cb;
}