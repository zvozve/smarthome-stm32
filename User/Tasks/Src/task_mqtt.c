#include "task_mqtt.h"
#include "network_config.h"
#include "mqtt_client.h"
#include "mqtt_transport.h"
#include "cJSON.h"
#include "hlk_rm58s_port.h"
#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_client.h"

#include <string.h>
#include <stdlib.h>

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#define MQTT_TASK_LOG(fmt, ...) HLK_LOG("[MQTT] " fmt, ##__VA_ARGS__)

static mqtt_client_t mqtt_client;
static bool mqtt_ready = false;

static void on_mqtt_message(const char *topic, uint8_t *payload, uint16_t len) {
    char json_str[512];
    if (len < sizeof(json_str)) {
        memcpy(json_str, payload, len);
        json_str[len] = '\0';
        MQTT_TASK_LOG("Recv: topic=%s, payload=%s\n", topic, json_str);

        cJSON *root = cJSON_Parse(json_str);
        if (root) {
            cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
            if (cmd && cJSON_IsString(cmd)) {
                MQTT_TASK_LOG("Command: %s\n", cmd->valuestring);
            }
            cJSON_Delete(root);
        }
    }
}

static void on_mqtt_connect(bool success) {
    if (success) {
        mqtt_ready = true;
        MQTT_TASK_LOG("Connected!\n");
        mqtt_client_subscribe(&mqtt_client, "device/control", 1);
        MQTT_TASK_LOG("Subscribed to device/control\n");
        mqtt_client_publish(&mqtt_client, "device/status", "{\"status\":\"online\"}");
    } else {
        mqtt_ready = false;
        MQTT_TASK_LOG("Connect failed\n");
    }
}

void task_mqtt(void *argument) {
    (void)argument;
    MQTT_TASK_LOG("Task started\n");

    mqtt_client_init(&mqtt_client, &wifi_transport);
    mqtt_client_set_publish_cb(&mqtt_client, on_mqtt_message);
    mqtt_client_set_connect_cb(&mqtt_client, on_mqtt_connect);

    mqtt_config_t config = {
        .client_id = MQTT_CLIENT_ID,
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .keep_alive = 60,
        .clean_session = 1,
    };

    while (1) {
        if (!mqtt_ready || !mqtt_client_is_connected(&mqtt_client)) {
            // 等待 WiFi 就绪
            if (!HLK_Client_IsReady()) {
                MQTT_TASK_LOG("Waiting for WiFi...\n");
                HLK_Delay_ms(2000);
                continue;
            }
            
            MQTT_TASK_LOG("Connecting to MQTT broker...\n");
            if (mqtt_client_connect(&mqtt_client, MQTT_BROKER_ADDR, MQTT_BROKER_PORT, &config) == 0) {
                MQTT_TASK_LOG("MQTT connected!\n");
            } else {
                MQTT_TASK_LOG("MQTT connect failed, retry in 5s\n");
                HLK_Delay_ms(5000);
                continue;
            }
        }

        mqtt_client_loop(&mqtt_client);

        static uint32_t last_report = 0;
        if (mqtt_ready && (HLK_GET_TICK() - last_report > 10000)) {
            last_report = HLK_GET_TICK();
        }

        HLK_Delay_ms(10);
    }
}

bool task_mqtt_is_connected(void) {
    return mqtt_ready && mqtt_client_is_connected(&mqtt_client);
}

int task_mqtt_publish(const char *topic, const char *payload) {
    if (!mqtt_ready) return -1;
    return mqtt_client_publish(&mqtt_client, topic, payload);
}