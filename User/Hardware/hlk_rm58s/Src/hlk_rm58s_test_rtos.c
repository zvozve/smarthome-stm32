#include "hlk_rm58s_test.h"
#include "hlk_rm58s_client.h"
#include "hlk_rm58s_port.h"
#include <string.h>

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

// ===========================
// RTOS配置
// ===========================
#define HLK_TEST_TASK_STACK_SIZE    512
#define HLK_TEST_TASK_PRIORITY      3
#define HLK_TEST_SEND_INTERVAL      1000

// ===========================
// 任务句柄
// ===========================
static TaskHandle_t hlk_test_task_handle = NULL;
static TaskHandle_t hlk_monitor_task_handle = NULL;

// ===========================
// 测试统计
// ===========================
static struct {
    uint32_t send_count;
    uint32_t recv_count;
    uint32_t error_count;
    uint32_t reconnect_count;
} test_stats = {0};

// ===========================
// 函数声明
// ===========================
static void hlk_test_task(void *pvParameters);
static void hlk_monitor_task(void *pvParameters);

// ===========================
// 用户回调
// ===========================
static void my_rx_handler(const char* data, uint16_t len) {
    test_stats.recv_count++;
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
    
    xTaskCreate(hlk_test_task, "HLK_Test", HLK_TEST_TASK_STACK_SIZE, NULL,
                HLK_TEST_TASK_PRIORITY, &hlk_test_task_handle);
    xTaskCreate(hlk_monitor_task, "HLK_Monitor", HLK_TEST_TASK_STACK_SIZE / 2, NULL,
                HLK_TEST_TASK_PRIORITY - 1, &hlk_monitor_task_handle);
    
    HLK_LOG("[TEST] RTOS mode initialized\n");
}

// ===========================
// 任务（RTOS模式为空）
// ===========================
void HLK_Test_Task(void) {
    return;
}

// ===========================
// 测试任务
// ===========================
static void hlk_test_task(void *pvParameters) {
    TickType_t last_send_time = xTaskGetTickCount();
    const TickType_t send_interval = pdMS_TO_TICKS(HLK_TEST_SEND_INTERVAL);
    
    HLK_LOG("[TEST] Test task started\n");
    
    while (1) {
        HLK_Client_Task();
        
        if (HLK_Client_IsReady()) {
            if ((xTaskGetTickCount() - last_send_time) >= send_interval) {
                last_send_time = xTaskGetTickCount();
                
                char json_buf[128];
                HLK_Client_BuildJSON(json_buf, sizeof(json_buf));
                
                if (HLK_Client_SendString(json_buf)) {
                    test_stats.send_count++;
                    HLK_LOG("[TX] #%d: %s\n", test_stats.send_count, json_buf);
                } else {
                    test_stats.error_count++;
                    HLK_LOG("[TX] Send failed!\n");
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ===========================
// 监控任务
// ===========================
static void hlk_monitor_task(void *pvParameters) {
    const TickType_t print_interval = pdMS_TO_TICKS(10000);
    
    HLK_LOG("[MONITOR] Monitor task started\n");
    
    while (1) {
        vTaskDelay(print_interval);
        
        HLK_LOG("\n========== HLK Test Stats ==========\n");
        HLK_LOG("Send Count:   %lu\n", test_stats.send_count);
        HLK_LOG("Recv Count:   %lu\n", test_stats.recv_count);
        HLK_LOG("Error Count:  %lu\n", test_stats.error_count);
        HLK_LOG("Reconnect:    %lu\n", test_stats.reconnect_count);
        HLK_LOG("Ready:        %s\n", HLK_Client_IsReady() ? "YES" : "NO");
        HLK_LOG("Status:       %d\n", HLK_Client_GetStatus());
        HLK_LOG("Free Heap:    %lu\n", xPortGetFreeHeapSize());
        HLK_LOG("=====================================\n\n");
    }
}

// ===========================
// RTOS外部接口
// ===========================
void HLK_Test_RTOS_Reconnect(void) {
    test_stats.reconnect_count++;
    HLK_Client_Reconnect();
    HLK_LOG("[TEST] Manual reconnect triggered\n");
}

void HLK_Test_RTOS_GetStats(uint32_t* send, uint32_t* recv, uint32_t* err, uint32_t* recon) {
    if (send) *send = test_stats.send_count;
    if (recv) *recv = test_stats.recv_count;
    if (err) *err = test_stats.error_count;
    if (recon) *recon = test_stats.reconnect_count;
}

void HLK_Test_RTOS_ResetStats(void) {
    memset(&test_stats, 0, sizeof(test_stats));
    HLK_LOG("[TEST] Stats reset\n");
}

TaskHandle_t HLK_Test_RTOS_GetTaskHandle(void) {
    return hlk_test_task_handle;
}

bool HLK_Test_RTOS_Send(const char* data) {
    if (!data) return false;
    if (!HLK_Client_IsReady()) return false;
    
    if (HLK_Client_SendString(data)) {
        test_stats.send_count++;
        return true;
    }
    return false;
}

#endif // HLK_USE_RTOS