#include "hlk_rm58s.h"
#include "hlk_rm58s_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "SEGGER_RTT.h"
#include "stm32f4xx_hal.h"

void hlk_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    // HAL_Delay(ms); 
}

uint32_t hlk_get_tick(void) {
    return xTaskGetTickCount();
    // return HAL_GetTick();  // 临时用 HAL_GetTick
}

int hlk_rm58s_rtos_init(hlk_config_t *config) {
    HLK_LOG("rtos_init entry\n");
    if (!config) {
        HLK_LOG("config is NULL\n");
        return -1;
    }
        
    int ret = hlk_rm58s_init(config);
    HLK_LOG("hlk_rm58s_init returned: %d\n", ret);
    return ret;
}

void hlk_rm58s_poll_task(void *pvParameters) {
    (void)pvParameters;
    while (1) {
        hlk_rm58s_poll();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}