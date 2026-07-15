#include "hlk_rm58s_test.h"
#include "hlk_rm58s_client.h"

#if !HLK_USE_RTOS
// ===========================
// 用户回调
// ===========================
static void my_rx_handler(const char* data, uint16_t len) {
    HLK_LOG("Received: %.*s\n", len, data);
}

static void my_status_handler(hlk_status_t status) {
    const char* status_str[] = {
        "IDLE", "CONFIGURING", "WAIT_WIFI", "WAIT_SOCKET", "READY", "RECONFIG"
    };
    const char* name = (status <= HLK_RECONFIG) ? status_str[status] : "UNKNOWN";
    HLK_LOG("Status: %s (%d)\n", name, status);
    
    if (status == HLK_READY) {
        HLK_LOG("WiFi module ready!\n");
    }
}

// ===========================
// 初始化
// ===========================
void HLK_Test_Init(void) {
    HLK_Client_Init();
    
    HLK_Client_SetRxCallback(my_rx_handler);
    HLK_Client_SetStatusCallback(my_status_handler);
    
    HLK_LOG("[TEST] Bare metal mode initialized\n");
}

// ===========================
// 任务（周期性调用）
// ===========================
void HLK_Test_Task(void) {
    HLK_Client_Task();
    
    if (HLK_Client_IsReady()) {
        HLK_Client_SendString("Hello Server");
    }
}
#endif
