#include "hlk_rm58s_app.h"
#include "hlk_rm58s_driver.h"

/* 外部变量声明 */
extern UART_HandleTypeDef huart_wifi;
extern Uart_Task_t Uart_Task_WiFi;

void hlk_rm58s_init(void) {
    wifiMaster_cmd_clr();

    Uart_Task_WiFi.status  = UTS_IDLE;

    #if UART_ENABLE_DMACPY
    HAL_DMA_RegisterCallback(&hdma_mb_cpy, HAL_DMA_XFER_CPLT_CB_ID, mbSlave_FIFO_Write_Regs_DMA_Callback);
    #endif // UART_ENABLE_DMACPY

    // 启动定时器轮询 10us
    HAL_TIM_Base_Start_IT(&htim_tp);

    // 进入AT模式配置WiFi模块 (对照HLK-RM58S规格书)
    hlk_rm58s_connect(WIFI_MODE_STA);

    // 配置完成后启动DMA接收, 进入透传模式
    HAL_UARTEx_ReceiveToIdle_DMA(&huart_wifi, Uart_Task_WiFi.Rx_Buf, UART_BUF_LEN);

    SEGGER_RTT_printf(0, "WiFi init done, entering transparent mode.\n");
}

void hlk_rm58s_task(void) {
    // 透传模式: 每秒发送一次JSON状态报告
    build_json_message(jsonTxBuf, sizeof(jsonTxBuf));
    memcpy(wifiMaster.Tx.cmdBuf, jsonTxBuf, strlen(jsonTxBuf));
    HAL_UART_Transmit_DMA(&huart_wifi, (uint8_t *)jsonTxBuf, strlen(jsonTxBuf));

    SEGGER_RTT_printf(0, "Send JSON: %s\n", jsonTxBuf);

    if (rx_flag == true) {
        rx_flag = false;
        parse_json(jsonRxBuf);
        SEGGER_RTT_printf(0, "Recv JSON: %s\n", jsonRxBuf);
    }
}

void wifiMaster_Poll(Uart_Task_t *task) {
//    USR_UART_Print_STA("wifiMaster_Poll ", task->status, wifiMaster.Status);
    switch (task->status) {
        // 静态
        case UTS_IDLE:
        break;
        // 构建请求，发送
        case UTS_REQUESTING:
            // mbMaster_Structuring_Request();
            memcpy(task->Tx_Buf, wifiMaster.Tx.cmdBuf, wifiMaster.Tx.cmdByte);
            task->Tx_Byte = wifiMaster.Tx.cmdByte;
            USR_UART_Transmit_DMA(task);
        break;
        // 发送完成: 打开接收，开始计时
        case UTS_TX_CPLT:
            USR_UART_Receive_DMA(task);
            task->timeout = TIM_TP_MAX_DELAY;
            task->retry   = 10;  // WiFi AT指令可能分多包返回(先回显再返回ok), 增加重试次数
        break;
        // 接收中：计时
        case UTS_RX_ING:
            if(task->timeout > 0) {
                task->timeout--;
            } else {
                if(task->retry > 0) {
                    task->retry--;
                    task->status = UTS_REQUESTING; // 重试请求
                } else {
                    task->status = UTS_RX_TIMEOUT;
                }
            }
        break;
        // 接收完成 — 累积多包数据, 直到收到完整响应
        // HLK-RM58S 先回显AT指令, 再返回 ok/error, 可能分多个DMA空闲包到达
        case UTS_RX_CPLT:
            // 累积接收数据到 wifiMaster.Rx.cmdBuf
            if (wifiMaster.Rx.cmdByte + task->Rx_Byte < UART_BUF_LEN) {
                memcpy(wifiMaster.Rx.cmdBuf + wifiMaster.Rx.cmdByte, task->Rx_Buf, task->Rx_Byte);
                wifiMaster.Rx.cmdByte += task->Rx_Byte;
                wifiMaster.Rx.cmdBuf[wifiMaster.Rx.cmdByte] = '\0';
            }

            // 检查响应是否完整 (包含 ok / error)
            if (strcasestr_local((char*)wifiMaster.Rx.cmdBuf, "ok") != NULL ||
                strcasestr_local((char*)wifiMaster.Rx.cmdBuf, "error") != NULL) {
                // 响应完整, 进入解析
                task->status = UTS_PARSING;
            } else if (task->retry > 0) {
                // 响应不完整, 重启DMA接收等待后续数据 (不清空累积缓冲)
                task->retry--;
                task->Rx_Byte = 0;
                HAL_UARTEx_ReceiveToIdle_DMA(task->Port, task->Rx_Buf, UART_BUF_LEN);
                task->status = UTS_RX_ING;
                task->timeout = TIM_TP_MAX_DELAY;
            } else {
                // 重试用尽, 超时
                SEGGER_RTT_printf(0, "RX accumulate timeout!\n");
                task->status = UTS_RX_TIMEOUT;
            }
        break;
        // 解析
        case UTS_PARSING:
            // mbMaster_Processing_Response();
            
            task->status = UTS_CALLBACK;
        break;
        // 处理回调任务
        case UTS_CALLBACK:
            wifiMaster_Callback();
            task->status = UTS_IDLE;
        break;

        // 异常处理
        case UTS_TX_ERR:
        case UTS_RX_ERR:
        case UTS_TX_TIMEOUT:
        case UTS_RX_TIMEOUT:
            // 终止当前DMA操作
            HAL_UART_Abort(task->Port);  
            task->status = UTS_IDLE;
        break;
        
        default:
        break;            
    }
}

void wifiMaster_Callback(void){
    // uint8_t  addr   = wifiMaster.Tx.addr;
    // uint16_t fxcode = wifiMaster.Tx.fxcode;
}

void USR_UART_Print_STA(const char *sFormat, uart_Status_e mbtStatus, wifiStatus_t Status) {
    #if UART_ENABLE_RTT_PRINTF
    // 先输出格式化的前缀信息
    SEGGER_RTT_WriteString(0, sFormat);

    SEGGER_RTT_printf(0, "uart_Status_e: %02X, mbFxStatus_t %02X\n", mbtStatus, Status);    
    #endif // UART_ENABLE_RTT_PRINTF                       
}

// 中断处理函数
void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&htim_tp);
    us_count ++;
    // SEGGER_RTT_printf(0, "TIM6_DAC_IRQHandler %d\n", us_count); 
    wifiMaster_Poll(&Uart_Task_WiFi);
}

// 回调函数
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == Uart_Task_WiFi.Port) {
        USR_UART_TxCpltCallback(&Uart_Task_WiFi);
    }
}

/* 注意：HAL_UARTEx_RxEventCallback已在barcode_scanner_driver.c中实现
 * 该函数会根据huart参数自动分发到对应的处理逻辑
 */


 
