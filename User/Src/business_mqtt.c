#include "business_mqtt.h"
#include "board_cfg.h"     /* BOARD_WOL_TARGET_MAC / BOARD_WOL_NETIF */
#include "wol.h"              // send_wol (WoL 协议驱动)
#include "hal_platform.h"     /* HAL_GetTick（不直接依赖芯片系列头） */

#include <string.h>
#include <stdlib.h>
#include "cmsis_os2.h"

// ===========================
// WoL 业务状态（重试策略归业务层）
// ===========================
#define WOL_RETRY_MAX     3
#define WOL_RETRY_MS      30000

static bool     wol_send_request   = false;   // 命令触发的发送请求
static bool     wol_failed         = false;   // 上次发送是否失败
static uint8_t  wol_retry_count   = 0;
static uint32_t wol_last_send_tick = 0;

static void wol_do_send(void) {
    WOL_LOG("Sending WOL...");
    wol_last_send_tick = HAL_GetTick();

    err_t result = send_wol(BOARD_WOL_TARGET_MAC, BOARD_WOL_NETIF);

    if (result == ERR_OK) {
        WOL_LOG("WOL sent successfully");
        wol_failed = false;
        wol_retry_count = 0;
    } else {
        WOL_LOG("WOL failed (err=%d)", result);
        wol_failed = true;
        wol_retry_count++;
        if (wol_retry_count >= WOL_RETRY_MAX) {
            WOL_LOG("WOL failed %d times, stop retry", WOL_RETRY_MAX);
        }
    }
}

// ===========================
// 业务实现（用户只改这里）
// ===========================

// --- 命令 / 数据请求 / 配置 处理 ---
static void mqtt_business_on_command(const char *cmd, const char *params_json) {
    MQTT_LOG("MQTT cmd: %s | params: %s",
             cmd ? cmd : "(null)", params_json ? params_json : "(null)");

    /* 命令路由：业务层直接处理，无需独立任务 */
    if (strcmp(cmd, "wol") == 0) {
        wol_send_request = true;
        return;
    }
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

// ===========================
// 业务周期处理（由 MQTT 任务循环每 100ms 调用）
// WoL 重试等与 MQTT 连接状态独立的业务
// ===========================
void business_mqtt_process(void) {
    /* WoL：命令触发的首次发送 */
    if (wol_send_request) {
        wol_send_request = false;
        wol_do_send();
        return;
    }

    /* WoL：失败重试（每 30s，最多 3 次） */
    if (wol_failed && wol_retry_count < WOL_RETRY_MAX) {
        uint32_t now = HAL_GetTick();
        if (now - wol_last_send_tick > WOL_RETRY_MS) {
            WOL_LOG("Retry sending WOL (%d/%d)", wol_retry_count + 1, WOL_RETRY_MAX);
            wol_do_send();
        }
    }
}
