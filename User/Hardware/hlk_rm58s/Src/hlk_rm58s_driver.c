#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_port.h"
#include <string.h>

// ===========================
// 全局变量
// ===========================
hlk_task_t hlk_task = {0};

// ===========================
// GPIO 操作
// ===========================
void HLK_GPIO_Init(void) {
    WIFI_ES_HIGH();
    WIFI_RST_HIGH();
    HLK_LOG("GPIOInit done\n");
}

// 进入AT模式：拉低ES0
// 手册要求：拉低 ≥ 50ms 进入AT模式
void HLK_GPIO_EnterATMode(void) {
    WIFI_ES_LOW();
    HLK_Delay_ms(100);
    WIFI_ES_HIGH();
    HLK_Delay_ms(100);
    HLK_LOG("EnterATMode\n");
}

void HLK_GPIO_ActiveReset(void) {
    WIFI_RST_LOW();
    HLK_Delay_ms(600);
    WIFI_RST_HIGH();
    // 手册要求：复位后需要等待模块启动
    HLK_Delay_ms(100);  // 增加复位后等待
    HLK_LOG("ActiveReset done\n");
}

void HLK_GPIO_ActiveRecoveryMode(void) {
    WIFI_ES_LOW();
    for (int i = 9; i > 0; i--) {
        HLK_LOG("Wait %d s...\n", i);
        HLK_Delay_ms(1000);
    }
    WIFI_ES_HIGH();
    HLK_Delay_ms(1000);
    HLK_LOG("ActiveRecoveryMode done\n");
}

// ===========================
// 启动接收
// ===========================
static void HLK_StartReceive(void) {
    if (hlk_task.status == HLK_BUSY_RX) {
        return;
    }
    
    HAL_UART_AbortReceive(&huart_hlk);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_ORE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_FE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_NE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_PE);
    
    memset(hlk_task.rxBuf, 0, HLK_UART_BUF_LEN);
    hlk_task.rxLen = 0;
    
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart_hlk, hlk_task.rxBuf, HLK_UART_BUF_LEN) != HAL_OK) {
        hlk_task.status = HLK_ERROR;
        if (hlk_task.on_error) hlk_task.on_error();
        return;
    }
    
    hlk_task.status = HLK_BUSY_RX;
}

// ===========================
// 驱动初始化
// ===========================
void HLK_Driver_Init(void) {
    memset(&hlk_task, 0, sizeof(hlk_task_t));
    hlk_task.status = HLK_IDLE;
    
    HAL_UART_Abort(&huart_hlk);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_ORE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_FE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_NE);
    __HAL_UART_CLEAR_FLAG(&huart_hlk, UART_FLAG_PE);
    
    // 默认启动接收
    HLK_StartReceive();
    HLK_LOG("Driver initialized\n");
}

// ===========================
// 发送数据
// ===========================
int HLK_Driver_Send(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0 || len > HLK_UART_BUF_LEN) {
        return -1;
    }

    if (hlk_task.status == HLK_ERROR) {
        HAL_UART_Abort(&huart_hlk);
        hlk_task.status = HLK_IDLE;
    }

    // 发送前先停接收
    if (hlk_task.status == HLK_BUSY_RX) {
        HAL_UART_AbortReceive(&huart_hlk);
        hlk_task.status = HLK_IDLE;
    }

    if (hlk_task.status == HLK_BUSY_TX) {
        return -2;
    }
    
    memcpy(hlk_task.txBuf, data, len);
    hlk_task.txLen = len;
    
    if (HAL_UART_Transmit_DMA(&huart_hlk, hlk_task.txBuf, len) != HAL_OK) {
        hlk_task.status = HLK_ERROR;
        if (hlk_task.on_error) hlk_task.on_error();
        return -3;
    }
    
    hlk_task.status = HLK_BUSY_TX;
    HLK_LOG("TX: %d bytes\n", len);
    return 0;
}

// ===========================
// 注册回调
// ===========================
void HLK_Driver_RegisterCallbacks(
    void (*on_rx)(uint8_t *data, uint16_t len),
    void (*on_tx)(void),
    void (*on_err)(void)
) {
    hlk_task.on_rx_complete = on_rx;
    hlk_task.on_tx_complete = on_tx;
    hlk_task.on_error = on_err;
}

// ===========================
// 发送完成回调
// ===========================
void HLK_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart_hlk && hlk_task.status == HLK_BUSY_TX) {
        hlk_task.status = HLK_IDLE;
        HLK_LOG("TX complete\n");
        
        if (hlk_task.on_tx_complete) {
            hlk_task.on_tx_complete();
        }
        
        // 发完继续接收
        HLK_StartReceive();
    }
}

// ===========================
// 接收完成回调
// ===========================
void HLK_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart == &huart_hlk && hlk_task.status == HLK_BUSY_RX) {
        hlk_task.rxLen = Size;
        hlk_task.rxBuf[Size] = '\0';
        hlk_task.status = HLK_IDLE;
        
        HLK_LOG("RX: %d bytes\n", Size);
                
        if (hlk_task.on_rx_complete) {
            hlk_task.on_rx_complete(hlk_task.rxBuf, Size);
        }
        
        // 收完继续接收
        HLK_StartReceive();
    }
}

// ===========================
// 错误处理
// ===========================
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart_hlk) {
        HLK_LOG("UART Error: 0x%08X\n", huart->ErrorCode);
        
        HAL_UART_Abort(huart);
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE);
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_FE);
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_NE);
        __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_PE);
        
        hlk_task.status = HLK_IDLE;
        hlk_task.rxLen = 0;
        hlk_task.txLen = 0;
        
        if (hlk_task.on_error) {
            hlk_task.on_error();
        }
        
        // 错误恢复后继续接收
        HLK_StartReceive();
    }
}