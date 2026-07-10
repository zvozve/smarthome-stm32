/**
 * @file hlk_rm58s_user_example.c
 * @brief 用户使用示例（可删除或参考修改）
 */

#include "network_config.h"
#include "hlk_rm58s.h"
#include "hlk_rm58s_bare.h"

// ===========================
// 工具函数
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
// 回调函数
// ===========================

static void on_rx(uint8_t *data, uint16_t len) {
    // 用户处理接收数据
    // xQueueSend(mqtt_rx_queue, data, 0);
}

static void on_state(bool wifi, bool tcp) {
    // 用户处理状态变化
}

// ===========================
// 初始化（裸机模式）
// ===========================

void app_wifi_init(void) {
    extern UART_HandleTypeDef huart6;

    hlk_config_t config = {
        .uart_handle = &huart6,
        .gpio_es_port = WIFI_ES_GPIO_Port,
        .gpio_es_pin = WIFI_ES_Pin,
        .gpio_rst_port = WIFI_RST_GPIO_Port,
        .gpio_rst_pin = WIFI_RST_Pin,
        .gpio_sts_port = WIFI_STS_GPIO_Port,
        .gpio_sts_pin = WIFI_STS_Pin,
        .gpio_sot_port = WIFI_SOT_GPIO_Port,
        .gpio_sot_pin = WIFI_SOT_Pin,
        .rx_callback = on_rx,
        .state_callback = on_state,
        .at_timeout_ms = 2000,
        .connect_timeout_ms = 10000,
    };

    hlk_rm58s_bare_init(&config);

    // 从 network_config.h 读取参数
    hlk_rm58s_connect_sta(WIFI_SSID, WIFI_PASSWORD, get_security_type());
    hlk_rm58s_config_tcp(REMOTE_IP, atoi(REMOTE_PORT));
    hlk_rm58s_commit_and_reboot();
}