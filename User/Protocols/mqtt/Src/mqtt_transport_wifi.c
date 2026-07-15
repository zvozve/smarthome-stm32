#include "mqtt_transport.h"
#include "hlk_rm58s_client.h"   // 改用 client 接口
#include "hlk_rm58s_port.h"     // 使用 HLK_LOG
#include <string.h>

#define MQTT_TRANSPORT_LOG(fmt, ...) HLK_LOG("[MQTT_TRANSPORT] " fmt, ##__VA_ARGS__)

static mqtt_transport_rx_cb_t app_rx_callback = NULL;
static bool wifi_tcp_connected = false;

// ===========================
// WiFi 接收回调（由 HLK_Client 调用）
// ===========================
static void wifi_rx_callback(const char* data, uint16_t len) {
    if (app_rx_callback) {
        app_rx_callback((uint8_t*)data, len);
    }
}

// ===========================
// 传输层接口实现
// ===========================
static int wifi_transport_init(mqtt_transport_rx_cb_t rx_callback) {
    app_rx_callback = rx_callback;
    
    // 注册接收回调到 Client
    HLK_Client_SetRxCallback(wifi_rx_callback);
    
    MQTT_TRANSPORT_LOG("Init OK\n");
    return 0;
}

static void wifi_transport_deinit(void) {
    app_rx_callback = NULL;
    HLK_Client_SetRxCallback(NULL);
    MQTT_TRANSPORT_LOG("Deinit\n");
}

static int wifi_transport_connect(const char *host, uint16_t port) {
    MQTT_TRANSPORT_LOG("Connecting to %s:%d\n", host, port);
    
    // 等待 WiFi 就绪
    if (!HLK_Client_IsReady()) {
        MQTT_TRANSPORT_LOG("WiFi not ready\n");
        return -1;
    }
    
    wifi_tcp_connected = true;
    MQTT_TRANSPORT_LOG("TCP connected!\n");
    return 0;
}

static void wifi_transport_disconnect(void) {
    wifi_tcp_connected = false;
    MQTT_TRANSPORT_LOG("Disconnected\n");
}

static bool wifi_transport_is_connected(void) {
    return wifi_tcp_connected && HLK_Client_IsReady();
}

static int wifi_transport_send(uint8_t *data, uint16_t len) {
    if (!wifi_tcp_connected) return -1;
    
    if (HLK_Client_Send((const char*)data, len)) {
        return len;
    }
    return -1;
}

static int wifi_transport_recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms) {
    // 裸机环境下由回调处理，这里返回0表示非阻塞
    // 实际数据在 wifi_rx_callback 中处理
    return 0;
}

static void wifi_transport_reset(void) {
    wifi_transport_disconnect();
    HLK_Client_Reconnect();
    MQTT_TRANSPORT_LOG("Reset\n");
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