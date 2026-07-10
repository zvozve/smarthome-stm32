#include "hlk_rm58s.h"
#include "stm32f4xx_hal.h"

int hlk_rm58s_bare_init(hlk_config_t *config) {
    if (!config) return -1;
    return hlk_rm58s_init(config);
}

void hlk_rm58s_bare_loop(void) {
    hlk_rm58s_poll();
    HAL_Delay(10);
}