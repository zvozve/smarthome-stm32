#include "mqtt_codec.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

char* mqtt_status_to_json(const device_status_t *status) {  // ← 改成 device_status_t
    if (!status) return NULL;
    
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    
    cJSON_AddNumberToObject(root, "temperature", status->temperature);
    cJSON_AddNumberToObject(root, "humidity", status->humidity);
    cJSON_AddNumberToObject(root, "led", status->led_state);
    cJSON_AddNumberToObject(root, "relay", status->relay_state);
    cJSON_AddNumberToObject(root, "uptime", status->uptime);
    cJSON_AddStringToObject(root, "status", "online");
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

int mqtt_parse_command(const char *topic, const uint8_t *payload, uint16_t len,
                        char *cmd_buf, size_t cmd_buf_size, char **params_out) {
    if (!topic || !cmd_buf) return -1;
    
    const char *prefix = "smart_home/cmd/";
    if (strncmp(topic, prefix, 15) != 0) return -1;
    
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
    
    return 0;
}