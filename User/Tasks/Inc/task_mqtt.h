#ifndef __TASK_MQTT_H
#define __TASK_MQTT_H

#include <stdbool.h>

/**
 * @brief MQTT任务函数（由osThreadNew创建）
 */
void task_mqtt(void *argument);

/**
 * @brief MQTT是否已连接
 */
bool task_mqtt_is_connected(void);

/**
 * @brief 通过MQTT发送数据
 */
int task_mqtt_publish(const char *topic, const char *payload);

#endif /* __TASK_MQTT_H */