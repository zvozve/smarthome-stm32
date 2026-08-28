#ifndef __TASK_WIFI_H
#define __TASK_WIFI_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*wifi_rx_callback_t)(const uint8_t *data, uint16_t len, void *ctx);
typedef void (*wifi_state_callback_t)(bool connected, void *ctx);

bool Task_WiFi_init(void);
bool Task_WiFi_is_connected(void);
int Task_WiFi_send(const uint8_t *data, uint16_t len);

int Task_WiFi_register_rx_callback(wifi_rx_callback_t callback, void *ctx);
int Task_WiFi_unregister_rx_callback(wifi_rx_callback_t callback);
int Task_WiFi_register_state_callback(wifi_state_callback_t callback, void *ctx);

#endif