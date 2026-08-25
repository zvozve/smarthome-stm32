#ifndef __BUSINESS_MQTT_H__
#define __BUSINESS_MQTT_H__

#include <stdint.h>
#include "mqtt_codec.h"    // device_status_t
#include "mqtt_client.h"   // MQTT_LOG

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// 业务主题 (用户拥有：订阅什么 / 发布到哪)
// ===========================
#define MQTT_TOPIC_SUBSCRIBE  "smart_home/cmd/#"
#define MQTT_TOPIC_STATUS     "smart_home/status"
#define MQTT_TOPIC_DATA       "smart_home/data"
#define MQTT_TOPIC_EVENT      "smart_home/event"
#define MQTT_TOPIC_CONFIG     "smart_home/config"

// ===========================
// 业务接口 (移植时只改这个文件)
// ===========================
// 收到消息：控制解析 + 路由（cmd / data / config）
void mqtt_business_on_message(const char *topic, const uint8_t *payload, uint16_t len);

// 设备状态（心跳发什么）
void mqtt_business_get_device_status(device_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* __BUSINESS_MQTT_H__ */
