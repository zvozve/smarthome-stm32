/**
 * @file task_wifi.c
 * @brief WiFi管理任务
 */

#include "task_wifi.h"
#include "network_config.h"
#include "hlk_rm58s.h"
#include "hlk_rm58s_rtos.h"

#include <string.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "stm32f4xx_hal.h"

// ===========================
// 外部变量（CubeMX生成）
// ===========================

extern UART_HandleTypeDef huart6;

// ===========================
// 事件标志
// ===========================

#define WIFI_EVENT_CONNECTED    (1 << 0)
#define WIFI_EVENT_TCP_CONNECTED (1 << 1)

static StaticEventGroup_t wifi_event_group_buffer; 
static EventGroupHandle_t wifi_event_group = NULL;

// ===========================
// 状态缓存
// ===========================

static bool wifi_connected = false;
static bool tcp_connected = false;
static hlk_state_t current_state = HLK_STATE_UNINIT;

// ===========================
// 内部工具函数
// ===========================

static hlk_security_t get_security_type(void) {
    if (strcmp(WIFI_SECURITY, "none") == 0) return HLK_SECURITY_NONE;
    if (strcmp(WIFI_SECURITY, "wep") == 0) return HLK_SECURITY_WEP;
    if (strcmp(WIFI_SECURITY, "wpa_psk") == 0) return HLK_SECURITY_WPA_PSK;
    if (strcmp(WIFI_SECURITY, "wpa2_psk") == 0) return HLK_SECURITY_WPA2_PSK;
    if (strcmp(WIFI_SECURITY, "wpa_wpa2_psk") == 0) return HLK_SECURITY_WPA_WPA2_PSK;
    return HLK_SECURITY_WPA2_PSK;
}

// ===========================
// 驱动回调函数
// ===========================

static void on_wifi_rx(uint8_t *data, uint16_t len) {
    // 透传数据接收 → 发送给MQTT任务
}

static void on_wifi_state(bool wifi, bool tcp) {
    wifi_connected = wifi;
    tcp_connected = tcp;
    current_state = hlk_rm58s_get_state();

    if (wifi) {
        xEventGroupSetBits(wifi_event_group, WIFI_EVENT_CONNECTED);
    } else {
        xEventGroupClearBits(wifi_event_group, WIFI_EVENT_CONNECTED);
    }

    if (tcp) {
        xEventGroupSetBits(wifi_event_group, WIFI_EVENT_TCP_CONNECTED);
    } else {
        xEventGroupClearBits(wifi_event_group, WIFI_EVENT_TCP_CONNECTED);
    }

    WIFI_LOG("State: WiFi=%d, TCP=%d, Mode=%d\r\n", wifi, tcp, current_state);
}

// ===========================
// WiFi配置和连接
// ===========================

static void wifi_connect(void) {
    WIFI_LOG("Connecting to SSID: %s\r\n", WIFI_SSID);

    // 1. 连接WiFi
    if (hlk_rm58s_connect_sta(WIFI_SSID, WIFI_PASSWORD, get_security_type()) != 0) {
        WIFI_LOG("Connect STA failed\r\n");
        return;
    }

    // 2. 配置TCP服务器
    if (hlk_rm58s_config_tcp(REMOTE_IP, atoi(REMOTE_PORT)) != 0) {
        WIFI_LOG("Config TCP failed\r\n");
        return;
    }

    // 3. 提交并重启
    if (hlk_rm58s_commit_and_reboot() != 0) {
        WIFI_LOG("Commit failed\r\n");
        return;
    }

    WIFI_LOG("Config sent, waiting for connection...\r\n");
}

// ===========================
// WiFi任务（由osThreadNew创建）
// ===========================

void task_wifi(void *argument) {
    (void)argument;
    WIFI_LOG("wifiTask entry 1\r\n");
    
    WIFI_LOG("Free heap before event group: %d\r\n", xPortGetFreeHeapSize());
    
    wifi_event_group = xEventGroupCreateStatic(&wifi_event_group_buffer);
    WIFI_LOG("Event group created: %p\r\n", wifi_event_group);

    // ----- 1. 创建事件组 -----
    // wifi_event_group = xEventGroupCreate();
    // if (!wifi_event_group) {
    //     WIFI_LOG("Failed to create event group\r\n");
    //     while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    // }

    // ----- 2. 配置驱动 -----
    hlk_config_t config = {
        .uart_handle = &huart6,
        .rx_callback = on_wifi_rx,
        .state_callback = on_wifi_state,
        .at_timeout_ms = 2000,
        .connect_timeout_ms = 10000,
    };

    if (hlk_rm58s_rtos_init(&config) != 0) {
        WIFI_LOG("Driver init failed\r\n");
        while(1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    WIFI_LOG("Driver init OK\r\n");

    // ----- 3. 首次连接 -----
    wifi_connect();

    // ----- 4. 主循环 -----
    while (1) {
        // 等待WiFi连接（超时30秒）
        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_EVENT_CONNECTED,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(30000)
        );

        if (bits & WIFI_EVENT_CONNECTED) {
            WIFI_LOG("WiFi connected!\r\n");
        } else {
            WIFI_LOG("WiFi connect timeout, retry...\r\n");
            wifi_connect();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        // 等待TCP连接（超时30秒）
        bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_EVENT_TCP_CONNECTED,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(30000)
        );

        if (bits & WIFI_EVENT_TCP_CONNECTED) {
            WIFI_LOG("TCP connected! Ready for MQTT\r\n");
        } else {
            WIFI_LOG("TCP connect timeout\r\n");
        }

        // ----- 5. 状态监控 -----
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(10000));

            // 检查WiFi状态
            if (!hlk_rm58s_is_wifi_connected()) {
                WIFI_LOG("WiFi lost, reconnecting...\r\n");
                break;  // 跳出，重新连接
            }

            // 检查TCP状态
            if (!hlk_rm58s_is_tcp_connected()) {
                WIFI_LOG("TCP lost, waiting...\r\n");
                EventBits_t tcp_bits = xEventGroupWaitBits(
                    wifi_event_group,
                    WIFI_EVENT_TCP_CONNECTED,
                    pdFALSE,
                    pdTRUE,
                    pdMS_TO_TICKS(30000)
                );
                if (tcp_bits & WIFI_EVENT_TCP_CONNECTED) {
                    WIFI_LOG("TCP reconnected!\r\n");
                }
            }

            // 打印RSSI（调试）
            int rssi = hlk_rm58s_get_rssi();
            if (rssi != -1) {
                WIFI_LOG("RSSI: %d dBm\r\n", rssi);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ===========================
// 状态查询接口
// ===========================

bool task_wifi_is_connected(void) {
    return wifi_connected;
}

bool task_wifi_is_tcp_connected(void) {
    return tcp_connected;
}