#include "business_mqtt.h"

#include <string.h>
#include <stdlib.h>
#include "cmsis_os2.h"

// ===========================
// 业务实现（用户只改这里）
// ===========================

// --- 命令 / 数据请求 / 配置 处理 ---
static void mqtt_business_on_command(const char *cmd, const char *params_json) {
    MQTT_LOG("MQTT cmd: %s | params: %s",
             cmd ? cmd : "(null)", params_json ? params_json : "(null)");
    /* TODO: 路由到具体设备动作（LED / 继电器 / 场景等） */
}

static void mqtt_business_on_data_request(const char *type) {
    MQTT_LOG("MQTT data request: %s", type ? type : "(null)");
}

static void mqtt_business_on_config(const char *config_json) {
    MQTT_LOG("MQTT config: %s", config_json ? config_json : "(null)");
}

// 控制解析：剥离 "smart_home/cmd/" 前缀，提取命令名与参数(JSON)
static void mqtt_business_parse_control(const char *topic, const uint8_t *payload, uint16_t len,
                                        char *cmd_buf, size_t cmd_buf_size, char **params_out) {
    const char *prefix = "smart_home/cmd/";
    if (strncmp(topic, prefix, 15) != 0) return;
    const char *cmd = topic + 15;
    strncpy(cmd_buf, cmd, cmd_buf_size - 1);
    cmd_buf[cmd_buf_size - 1] = '\0';
    if (payload && len > 0 && params_out) {
        char *json_str = (char*)malloc(len + 1);
        if (json_str) {
            memcpy(json_str, payload, len);
            json_str[len] = '\0';
            *params_out = json_str;
        }
    }
}

// 消息入口：解析 + 路由（属于业务层职责）
void mqtt_business_on_message(const char *topic, const uint8_t *payload, uint16_t len) {
    // 命令: smart_home/cmd/xxx
    if (strncmp(topic, "smart_home/cmd/", 15) == 0) {
        char cmd_buf[32];
        char *params = NULL;
        mqtt_business_parse_control(topic, payload, len, cmd_buf, sizeof(cmd_buf), &params);
        mqtt_business_on_command(cmd_buf, params);
        if (params) free(params);
        return;
    }

    // 数据请求: smart_home/data/xxx
    if (strncmp(topic, "smart_home/data/", 16) == 0) {
        mqtt_business_on_data_request(topic + 16);
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
        mqtt_business_on_config(json_str);
        if (json_str) free(json_str);
    }
}

// 设备状态（心跳发什么）
void mqtt_business_get_device_status(device_status_t *status) {
    if (!status) return;
    status->temperature = 0.0f;
    status->humidity    = 0.0f;
    status->led_state   = 0;
    status->relay_state = 0;
    status->uptime      = osKernelGetTickCount() / 1000;
}
