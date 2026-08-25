#ifndef __TASK_WIFI_H
#define __TASK_WIFI_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*wifi_rx_callback_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*wifi_state_callback_t)(bool connected, void *ctx);

bool task_wifi_init(void);
bool task_wifi_is_connected(void);
int task_wifi_send(const uint8_t *data, uint16_t len);

int task_wifi_register_rx_callback(wifi_rx_callback_t callback, void *ctx);
int task_wifi_unregister_rx_callback(wifi_rx_callback_t callback);
int task_wifi_register_state_callback(wifi_state_callback_t callback, void *ctx);

#endif