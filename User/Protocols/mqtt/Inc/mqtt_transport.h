#ifndef __MQTT_TRANSPORT_H
#define __MQTT_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// 传输层回调函数类型
// ===========================

typedef void (*mqtt_transport_rx_cb_t)(uint8_t *data, uint16_t len);

// ===========================
// 传输层接口
// ===========================

typedef struct {
    int (*init)(mqtt_transport_rx_cb_t rx_callback);
    void (*deinit)(void);
    int (*connect)(const char *host, uint16_t port);
    void (*disconnect)(void);
    bool (*is_connected)(void);
    int (*send)(uint8_t *data, uint16_t len);
    int (*recv)(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms);
    void (*reset)(void);
} mqtt_transport_t;

// ===========================
// 全局传输实例
// ===========================

extern mqtt_transport_t wifi_transport;

#ifdef __cplusplus
}
#endif

#endif /* __MQTT_TRANSPORT_H */