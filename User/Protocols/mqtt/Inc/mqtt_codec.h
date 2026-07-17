#ifndef __MQTT_CODEC_H__
#define __MQTT_CODEC_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 统一使用 device_status_t
typedef struct {
    float temperature;
    float humidity;
    uint8_t led_state;
    uint8_t relay_state;
    uint32_t uptime;
} device_status_t;

char* mqtt_status_to_json(const device_status_t *status);  // ← 改成 device_status_t
int mqtt_parse_command(const char *topic, const uint8_t *payload, uint16_t len, 
                        char *cmd_buf, size_t cmd_buf_size, char **params_out);

#ifdef __cplusplus
}
#endif

#endif