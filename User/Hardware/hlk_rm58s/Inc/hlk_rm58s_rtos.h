#ifndef __HLK_RM58S_RTOS_H
#define __HLK_RM58S_RTOS_H

#include "hlk_rm58s.h"

/**
 * @brief RTOS模式初始化驱动（自动填充 hlk_delay_ms 和 hlk_get_tick）
 * @param config 配置结构体（只需填写硬件句柄和回调）
 * @return 0成功, -1失败
 */

void hlk_delay_ms(uint32_t ms);
uint32_t hlk_get_tick(void);
int hlk_rm58s_rtos_init(hlk_config_t *config);

/**
 * @brief WiFi轮询任务（FreeRTOS任务函数）
 * @param pvParameters 任务参数（未使用）
 */
void hlk_rm58s_poll_task(void *pvParameters);

#endif /* __HLK_RM58S_RTOS_H */