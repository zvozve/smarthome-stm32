#include "hlk_rm58s_func.h"

void hlk_rm58s_connect(WiFiMode_t mode) {
    hlk_rm58s_gpio_init();
    HAL_Delay(500);

    // 进入 AT 模式配置一次
    SEGGER_RTT_printf(0, "hlk_rm58s_configure_WiFi_Module!\n");
    hlk_rm58s_gpio_active_at_mode();
    hlk_rm58s_configure_WiFi_Module();

    // 等待 WiFi 连接
    while (!WIFI_STS_READ()) {
        for (int i = 60; i > 0; i--) {
            if (WIFI_STS_READ())
                break;
            SEGGER_RTT_printf(0, "Wait %d s...\n", i);
            HAL_Delay(1000);
        }
        if (WIFI_STS_READ())
            break;
        SEGGER_RTT_printf(0, "WiFi connect timeout!\n");

        // 进入 AT 模式配置一次
        SEGGER_RTT_printf(0, "hlk_rm58s_configure_WiFi_Module!\n");
        hlk_rm58s_gpio_active_at_mode();
        hlk_rm58s_configure_WiFi_Module();
    }
    SEGGER_RTT_printf(0, "WiFi connected.\n");
    // WIFI_LINK_ON();

    // 等待 TCP 连接
    while (!WIFI_SOT_READ()) {
        for (int i = 60; i > 0; i--) {
            if (WIFI_SOT_READ())
                break;
            SEGGER_RTT_printf(0, "Wait %d s...\n", i);
            HAL_Delay(1000);
        }
        
        if (WIFI_SOT_READ())
            break;

        SEGGER_RTT_printf(0, "Socket connect timeout!\n");
        // 进入 AT 模式配置一次
        SEGGER_RTT_printf(0, "hlk_rm58s_configure_WiFi_Module!\n");
        hlk_rm58s_gpio_active_at_mode();
        hlk_rm58s_configure_WiFi_Module();
    }
    SEGGER_RTT_printf(0, "Socket connected.\n");
    // WIFI_RX_TX_ON();
}
