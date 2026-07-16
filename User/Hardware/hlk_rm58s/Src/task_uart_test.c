#include "task_uart_test.h"
#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_port.h"
#include <string.h>

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

static struct {
    char last_rx[256];
    bool rx_ready;
    bool initialized;
} test = {0};

// ===========================
// 接收回调（驱动直接回调）
// ===========================
static void on_rx(uint8_t* data, uint16_t len) {
    if (len == 0 || data == NULL) return;
    uint16_t copy_len = len > sizeof(test.last_rx) - 1 ? sizeof(test.last_rx) - 1 : len;
    memcpy(test.last_rx, data, copy_len);
    test.last_rx[copy_len] = '\0';
    test.rx_ready = true;
    HLK_LOG("RX: %.*s\n", copy_len, test.last_rx);
}

// ===========================
// 发送完成回调
// ===========================
static void on_tx(void) {
    HLK_LOG("TX done\n");
}

// ===========================
// 错误回调
// ===========================
static void on_err(void) {
    HLK_LOG("UART error\n");
}

// ===========================
// 测试任务
// ===========================
static void test_task(void *arg) {
    (void)arg;
    
    // 初始化驱动
    HLK_Driver_Init();

    // 注册驱动回调
    HLK_Driver_RegisterCallbacks(on_rx, on_tx, on_err);
        
    uint32_t last_send = 0;
    while (1) {
        // 每3秒发一次AT
        if (HLK_GET_TICK() - last_send > 3000) {
            last_send = HLK_GET_TICK();
            HLK_Driver_Send((uint8_t*)"AT\r\n", 4);
            HLK_LOG("SEND: AT\n");
        }
        HLK_Delay_ms(50);
    }
}

// ===========================
// 初始化
// ===========================
void uart_test_init(void) {
    if (test.initialized) return;
    #if HLK_USE_RTOS
    xTaskCreate(test_task, "UARTTest", 2048, NULL, 3, NULL);
    #endif
    test.initialized = true;
}

// ===========================
// 发送命令（外部调用）
// ===========================
bool uart_test_send(const char *cmd, char *response, uint16_t len) {
    if (!test.initialized) return false;
    
    memset(test.last_rx, 0, sizeof(test.last_rx));
    test.rx_ready = false;
    
    uint8_t tx[128];
    snprintf((char*)tx, sizeof(tx), "%s\r\n", cmd);
    HLK_Driver_Send(tx, strlen((char*)tx));
    
    uint32_t start = HLK_GET_TICK();
    while ((HLK_GET_TICK() - start) < 3000) {
        if (test.rx_ready) {
            if (response && len > 0) {
                strncpy(response, test.last_rx, len - 1);
                response[len - 1] = '\0';
            }
            return true;
        }
        HLK_Delay_ms(10);
    }
    return false;
}