#include "mqtt_transport.h"
#include "stm32f4xx_hal.h"
#include "hlk_rm58s.h"
#include "SEGGER_RTT.h"

#define MQTT_TRANSPORT_LOG(fmt, ...) SEGGER_RTT_printf(0, "[MQTT_TRANSPORT] " fmt, ##__VA_ARGS__)

static mqtt_transport_rx_cb_t app_rx_callback = NULL;
static bool wifi_tcp_connected = false;

// ===========================
// WiFi 接收回调
// ===========================

static void wifi_rx_callback(uint8_t *data, uint16_t len) {
    if (app_rx_callback) {
        app_rx_callback(data, len);
    }
}

// ===========================
// 传输层接口实现
// ===========================

static int wifi_transport_init(mqtt_transport_rx_cb_t rx_callback) {
    app_rx_callback = rx_callback;
    hlk_rm58s_register_rx_callback(wifi_rx_callback);
    MQTT_TRANSPORT_LOG("Init OK\n");
    return 0;
}

static void wifi_transport_deinit(void) {
    app_rx_callback = NULL;
    hlk_rm58s_register_rx_callback(NULL);
    MQTT_TRANSPORT_LOG("Deinit\n");
}

static int wifi_transport_connect(const char *host, uint16_t port) {
    MQTT_TRANSPORT_LOG("Connecting to %s:%d\n", host, port);

    if (hlk_rm58s_config_tcp(host, port) != 0) {
        MQTT_TRANSPORT_LOG("Config TCP failed\n");
        return -1;
    }

    if (hlk_rm58s_commit_and_reboot() != 0) {
        MQTT_TRANSPORT_LOG("Commit failed\n");
        return -1;
    }

    // 等待 TCP 连接（使用 HAL_Delay）
    int timeout = 30;
    while (timeout-- > 0) {
        if (hlk_rm58s_is_tcp_connected()) {
            wifi_tcp_connected = true;
            MQTT_TRANSPORT_LOG("TCP connected!\n");
            return 0;
        }
        HAL_Delay(1000);  // 直接使用 HAL_Delay
    }

    MQTT_TRANSPORT_LOG("TCP connect timeout\n");
    return -1;
}

static void wifi_transport_disconnect(void) {
    wifi_tcp_connected = false;
    MQTT_TRANSPORT_LOG("Disconnected\n");
}

static bool wifi_transport_is_connected(void) {
    return wifi_tcp_connected && hlk_rm58s_is_tcp_connected();
}

static int wifi_transport_send(uint8_t *data, uint16_t len) {
    if (!wifi_tcp_connected) return -1;
    return hlk_rm58s_send(data, len);
}

static int wifi_transport_recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms) {
    return hlk_rm58s_recv(buf, max_len, timeout_ms);
}

static void wifi_transport_reset(void) {
    wifi_transport_disconnect();
    hlk_rm58s_reset(600);
}

// ===========================
// 全局传输实例
// ===========================

mqtt_transport_t wifi_transport = {
    .init = wifi_transport_init,
    .deinit = wifi_transport_deinit,
    .connect = wifi_transport_connect,
    .disconnect = wifi_transport_disconnect,
    .is_connected = wifi_transport_is_connected,
    .send = wifi_transport_send,
    .recv = wifi_transport_recv,
    .reset = wifi_transport_reset,
};