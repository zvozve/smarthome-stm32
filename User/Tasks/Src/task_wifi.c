#include "task_wifi.h"
#include "network_config.h"
#include "hlk_rm58s_client.h"   // 改用 client 接口
#include "hlk_rm58s_port.h"
#include "hlk_rm58s_driver.h"

#include <string.h>
#include <stdlib.h>

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#endif

// ===========================
// 状态缓存
// ===========================
static bool wifi_connected = false;
static bool tcp_connected = false;

static void on_wifi_state(hlk_status_t status) {
    wifi_connected = (status == HLK_READY);
    tcp_connected = (status == HLK_READY);
    
    WIFI_LOG("State: %d, WiFi=%d, TCP=%d\n", status, wifi_connected, tcp_connected);
}

// ===========================
// WiFi任务
// ===========================
void task_wifi(void *argument) {
    (void)argument;
    WIFI_LOG("Task started\n");
    
    // 注册状态回调
    HLK_Client_SetStatusCallback(on_wifi_state);
    
    // 初始化（HLK_Client 内部已包含 WiFi 配置）
    HLK_Client_Init();

    // 主循环
    while (1) {
        // 运行 Client 任务
        HLK_Client_Task();

        // 更新状态
        wifi_connected = HLK_Client_IsReady();
        tcp_connected = HLK_Client_IsReady();

        if (wifi_connected) {
            WIFI_LOG("WiFi/TCP connected!\n");
        }

        HLK_Delay_ms(1000);
    }
}

// ===========================
// 状态查询接口
// ===========================
bool task_wifi_is_connected(void) {
    return HLK_Client_IsReady();
}

bool task_wifi_is_tcp_connected(void) {
    return HLK_Client_IsReady();
}