#include "my_tim.h"

volatile uint32_t us_count = 0; // 在中断中递增的计数器

void TIM_TP_Delay_us(uint16_t us) {
    uint32_t start = us_count;
    int32_t ticks = us / TIM_TP_PERIOD_US; // 需要等待的ticks数
    while ((int32_t)(us_count - start) < ticks); // 正确处理溢出
}

// 毫秒级延时
void TIM_TP_Delay_ms(uint16_t ms) {
    for (uint16_t i = 0; i < ms; i++)
    {
        TIM_TP_Delay_us(1000);  // 1ms = 1000us
    }
}

#include "my_uart.h"

/* 全局UART缓冲区定义 */
usr_uart_t usr_uart1;  // UART1 - HMI
usr_uart_t usr_uart2;  // UART2 - PLC
usr_uart_t usr_uart6;  // UART6 - LED

#if UART_ENABLE_DMACPY
static FIFO_DMA_Context_t fifo_dma_ctx;
#endif // UART_ENABLE_DMACPY

// 统一的发送函数
void USR_UART_Transmit_DMA(Uart_Task_t *task) {
    if (task->status != UTS_TX_ING) {
        USR_UART_Print_CMD(task->debug_name, " TX CPLT: ", task->Tx_Buf, task->Tx_Byte);
        
        // RS485模式控制
        if (task->RS485_TX_Mode) {
            task->RS485_TX_Mode();
        }
        
        if (HAL_UART_Transmit_DMA(task->Port, task->Tx_Buf, task->Tx_Byte) != HAL_OK) {
            task->status = UTS_TX_ERR;
        } else {
            task->status = UTS_TX_ING;
        }
    }
}

// 统一的接收函数
void USR_UART_Receive_DMA(Uart_Task_t *task) {
    if (task->status != UTS_RX_ING) {
        memset(task->Rx_Buf, 0, UART_BUF_LEN);
        
        // RS485模式控制
        if (task->RS485_RX_Mode) {
            task->RS485_RX_Mode();
        }
        
        if (HAL_UARTEx_ReceiveToIdle_DMA(task->Port, task->Rx_Buf, UART_BUF_LEN) != HAL_OK) {
            task->status = UTS_RX_ERR;
        } else {
            task->status = UTS_RX_ING;
            USR_UART_Print_CMD(task->debug_name, " RX ING: ", task->Rx_Buf, task->Rx_Byte);
        }
    }
}

// 统一的发送完成回调
void USR_UART_TxCpltCallback(Uart_Task_t *task) {
    #if UART_ENABLE_RTT_PRINTF
    SEGGER_RTT_printf(0, "%s TX Complete!\n", task->debug_name);
    #endif
    
    if (task->status == UTS_TX_ING) {
        task->status = UTS_TX_CPLT;
    }
}

// 统一的接收事件回调
void USR_UARTEx_RxEventCallback(Uart_Task_t *task, uint16_t Size) {
    #if UART_ENABLE_RTT_PRINTF
    SEGGER_RTT_printf(0, "%s RX Event, Size: %d\n", task->debug_name, Size);
    #endif
    
    if (task->status == UTS_RX_ING) {
        task->Rx_Byte = Size;
        USR_UART_Print_CMD(task->debug_name, " RX CPLT: ", task->Rx_Buf, task->Rx_Byte);
        task->status = UTS_RX_CPLT;
    }
}

// 调试打印
void USR_UART_Print_CMD(const char *idFormat, const char *sFormat, uint8_t *pCmd, uint16_t len) {
    #if UART_ENABLE_RTT_PRINTF
    // 先输出格式化的前缀信息
    SEGGER_RTT_WriteString(0, idFormat);
    SEGGER_RTT_WriteString(0, sFormat);
    
    // 然后输出长度信息
    SEGGER_RTT_printf(0, "len %d\n", len);
    
    // 输出16进制数据
    for (uint16_t i = 0; i < len; i++) {
        SEGGER_RTT_printf(0, "%02X ", pCmd[i]);
    }
    SEGGER_RTT_printf(0, "\n");
    #endif // UART_ENABLE_RTT_PRINTF
}

uint8_t USR_UART_Unstable(UART_HandleTypeDef *huart) {
    if (huart->ErrorCode != HAL_UART_ERROR_NONE) {
        if (huart->ErrorCode & HAL_UART_ERROR_ORE) {
            SEGGER_RTT_printf(0, "USR_UART_Unstable: Overrun!\n");
            return 1;
        }
        if (huart->ErrorCode & HAL_UART_ERROR_NE)  {
            SEGGER_RTT_printf(0, "USR_UART_Unstable: Noise!\n");
            return 2;
        }
        if (huart->ErrorCode & HAL_UART_ERROR_FE)  {
            SEGGER_RTT_printf(0, "USR_UART_Unstable: Framing!\n");
            return 3;
        }
    }
    return 0;
}

void USR_UART_Repair(UART_HandleTypeDef *huart) {
    // 停止DMA传输
    HAL_UART_DMAStop(huart);
    
    // 根据芯片系列选择清除方式
    #if defined(USART_ISR_FE) // STM32G0/G4/H7等
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_FEF | UART_CLEAR_OREF);
    #elif defined(UART_SR_FE) // STM32F1/F4/L1等
        volatile uint32_t tmp = huart->Instance->SR;
        tmp = huart->Instance->DR;
        (void)tmp;
    #endif

    // 清除所有错误标志
    huart->ErrorCode = HAL_UART_ERROR_NONE;
    
    // 重新初始化串口
    HAL_UART_DeInit(huart);
    HAL_UART_Init(huart);
}
