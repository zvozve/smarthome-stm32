// hlk_rm58s_driver.c

#include "hlk_rm58s_driver.h"
#include <string.h>

// ===========================
// 全局变量
// ===========================
hlk_task_t hlk_task = {0};

// ===========================
// 字符串匹配
// ===========================
char *strcasestr_local(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++; n++;
        }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

// ===========================
// GPIO 操作
// ===========================
void HLK_GPIO_Init(void) {
    WIFI_ES_HIGH();
    WIFI_RST_HIGH();
}

void HLK_GPIO_ActiveATMode(void) {
    WIFI_ES_LOW();
    HLK_Delay_ms(1200);
    WIFI_ES_HIGH();
    HLK_Delay_ms(200);
    HLK_LOG("ActiveATMode\n");
}

void HLK_GPIO_ActiveReset(void) {
    WIFI_RST_LOW();
    HLK_Delay_ms(600);
    WIFI_RST_HIGH();
    HLK_Delay_ms(2000);
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
// 内部收发函数
// ===========================
static void HLK_UART_Transmit_DMA(hlk_task_t *task) {
    if (task->status != UTS_TX_ING) {
        HLK_LOG("TX: %s", task->txBuf);
        if (HAL_UART_Transmit_DMA(&huart_hlk, task->txBuf, task->txLen) != HAL_OK) {
            task->status = UTS_TX_ERR;
        } else {
            task->status = UTS_TX_ING;
        }
    }
}

static void HLK_UART_Receive_DMA(hlk_task_t *task) {
    if (task->status != UTS_RX_ING) {
        memset(task->rxBuf, 0, HLK_UART_BUF_LEN);
        if (HAL_UARTEx_ReceiveToIdle_DMA(&huart_hlk, task->rxBuf, HLK_UART_BUF_LEN) != HAL_OK) {
            task->status = UTS_RX_ERR;
        } else {
            task->status = UTS_RX_ING;
        }
    }
}

// ===========================
// 数据累积
// ===========================
static void HLK_AccumulateData(hlk_task_t *task) {
    if (task->rxLen == 0) return;
    
    if (task->rxAccumLen + task->rxLen < HLK_UART_BUF_LEN) {
        memcpy(task->rxAccumBuf + task->rxAccumLen, task->rxBuf, task->rxLen);
        task->rxAccumLen += task->rxLen;
        task->rxAccumBuf[task->rxAccumLen] = '\0';
        HLK_LOG("Accumulated: %d bytes (total: %d)\n", task->rxLen, task->rxAccumLen);
    }
}

static bool HLK_IsResponseComplete(hlk_task_t *task) {
    if (task->rxAccumLen == 0) return false;
    return (strcasestr_local((char*)task->rxAccumBuf, "ok") != NULL ||
            strcasestr_local((char*)task->rxAccumBuf, "error") != NULL);
}

// ===========================
// 清空命令
// ===========================
static void HLK_CleanCommand(void) {
    memset(hlk_task.txBuf, 0, HLK_UART_BUF_LEN);
    memset(hlk_task.rxBuf, 0, HLK_UART_BUF_LEN);
    memset(hlk_task.rxAccumBuf, 0, HLK_UART_BUF_LEN);
    hlk_task.txLen = 0;
    hlk_task.rxLen = 0;
    hlk_task.rxAccumLen = 0;
}

// ===========================
// 驱动层 API 实现
// ===========================
void HLK_Driver_Init(void) {
    HLK_CleanCommand();
    hlk_task.status = UTS_IDLE;
    hlk_task.timeout = HLK_MAX_DELAY_MS;
    hlk_task.retry = HLK_MAX_RERTY;
}

void HLK_Driver_RegisterCallbacks(void (*on_resp)(void), void (*on_to)(void), void (*on_err)(void)) {
    hlk_task.on_response_complete = on_resp;
    hlk_task.on_timeout = on_to;
    hlk_task.on_error = on_err;
}

void HLK_Driver_Transmit(const char *command) {
    char cmd_buf[128];
    size_t len = strlen(command);
    
    // 处理换行
    if (len > 0 && command[len - 1] == '\n') {
        strncpy(cmd_buf, command, sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    } else if (len > 0 && command[len - 1] == '\r') {
        snprintf(cmd_buf, sizeof(cmd_buf), "%s\n", command);
    } else {
        snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", command);
    }
    
    HLK_CleanCommand();
    hlk_task.txLen = strlen(cmd_buf);
    memcpy(hlk_task.txBuf, cmd_buf, hlk_task.txLen);
    hlk_task.status = UTS_REQUESTING;
}

// ===========================
// 驱动状态机（由定时器中断调用）
// ===========================
void HLK_Driver_Poll(void) {
    hlk_task_t *task = &hlk_task;
    
    // if (task->status != UTS_IDLE && task->status != UTS_RX_ING) {
    //     HLK_LOG("status %d\n", task->status);
    // }
    
    switch (task->status) {
        case UTS_IDLE:
            break;
            
        case UTS_REQUESTING:
            HLK_UART_Transmit_DMA(task);
            break;
            
        case UTS_TX_CPLT:
            if (hlk_task.is_at_mode) {
                HLK_UART_Receive_DMA(task);
                task->timeout = HLK_MAX_DELAY_MS;
                task->retry = HLK_MAX_RERTY;
            }
            break;

        case UTS_RX_ING:
            if (task->timeout > 0) {
                task->timeout--;
            } else {
                if (task->retry > 0) {
                    task->retry--;
                    task->status = UTS_REQUESTING;
                    HLK_LOG("RX timeout, retry: %d\n", task->retry);
                } else {
                    task->status = UTS_RX_TIMEOUT;
                    HLK_LOG("RX timeout!\n");
                    if (task->on_timeout) task->on_timeout();
                }
            }
            break;

        case UTS_RX_CPLT:
            HLK_AccumulateData(task);
            
            if (HLK_IsResponseComplete(task)) {
                task->status = UTS_PARSING;
            } else if (task->retry > 0) {
                task->retry--;
                task->rxLen = 0;
                HAL_UARTEx_ReceiveToIdle_DMA(&huart_hlk, task->rxBuf, HLK_UART_BUF_LEN);
                task->status = UTS_RX_ING;
                task->timeout = HLK_MAX_DELAY_MS;
                HLK_LOG("Waiting more data, retry: %d\n", task->retry);
            } else {
                HLK_LOG("RX accumulate timeout!\n");
                task->status = UTS_RX_TIMEOUT;
                if (task->on_timeout) task->on_timeout();
            }
            break;

        case UTS_PARSING:
            task->status = UTS_CALLBACK;
            break;

        case UTS_CALLBACK:
            if (task->on_response_complete) task->on_response_complete();
            task->status = UTS_IDLE;
            break;

        case UTS_TX_ERR:
        case UTS_RX_ERR:
        case UTS_TX_TIMEOUT:
        case UTS_RX_TIMEOUT:
            HAL_UART_Abort(&huart_hlk);
            if (task->on_error) task->on_error();
            task->status = UTS_IDLE;
            break;
            
        default:
            break;
    }
}

// 设置 AT 模式
void HLK_Driver_SetATMode(bool enable) {
    hlk_task.is_at_mode = enable;
}

bool HLK_Driver_GetATMode(void) {
    return hlk_task.is_at_mode;
}

// ===========================
// HAL 回调
// ===========================
void HLK_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart_hlk) {
        if (hlk_task.status == UTS_TX_ING) {
            hlk_task.status = UTS_TX_CPLT;
        }
    }
}

void HLK_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart == &huart_hlk) {
        // HLK_LOG("RX Event: %d bytes\n", Size);
        if (hlk_task.status == UTS_RX_ING) {
            hlk_task.rxLen = Size;
            hlk_task.rxBuf[Size] = '\0';
            HLK_LOG("RX: %s", hlk_task.rxBuf);
            hlk_task.status = UTS_RX_CPLT;
        }
    }
}

void HLK_TIMX_IRQHandler(TIM_HandleTypeDef *htim) {
    if (htim == &htim_hlk) {
        HLK_Driver_Poll();
    }
}

/*
在stm32f4xx_it.c中添加以下内容
修改为对应的串口和定时器

#include "hlk_rm58s_port.h"
#include "hlk_rm58s_driver.h"

void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

void TIM3_IRQHandler(void) {
  HAL_TIM_IRQHandler(&htim3);
  HLK_TIMX_IRQHandler(&htim_hlk);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
  HLK_UART_TxCpltCallback(huart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  HLK_UARTEx_RxEventCallback(huart, Size);
}

*/