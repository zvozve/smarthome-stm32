/**
 * @file    bsp_uart_drv.c
 * @brief   通用串口DMA驱动实现（修复版）
 * @version V3.1
 * @date    2026-08-24
 */

#include "bsp_uart_drv.h"
#include "usart.h"
#include "SEGGER_RTT_Log.h"
#include <string.h>

/* ===========================
 * 描述表
 * =========================== */
typedef struct {
    UART_HandleTypeDef *huart;
    const char *name;
    uint8_t index;
} uart_desc_t;

static const uart_desc_t g_uart_desc_table[] = {
    {&huart1, "UART1", 1},
    {&huart2, "UART2", 2},
    {&huart3, "UART3", 3},
    {&huart6, "UART6", 6},
    {NULL, NULL, 0}
};

static const uart_desc_t* uart_find_desc(UART_HandleTypeDef *huart) {
    for (int i = 0; g_uart_desc_table[i].huart != NULL; i++) {
        if (g_uart_desc_table[i].huart == huart) {
            return &g_uart_desc_table[i];
        }
    }
    return NULL;
}

const char* uart_drv_get_name(UART_HandleTypeDef *huart) {
    const uart_desc_t *desc = uart_find_desc(huart);
    return desc ? desc->name : "UNKNOWN";
}

uint8_t uart_drv_get_index(UART_HandleTypeDef *huart) {
    const uart_desc_t *desc = uart_find_desc(huart);
    return desc ? desc->index : 0;
}

/* ===========================
 * 实例管理
 * =========================== */
static uart_drv_t *s_drv_table[UART_DRV_MAX_INSTANCES];
static int s_drv_count = 0;

static void uart_drv_register_instance(uart_drv_t *drv) {
    if (s_drv_count < UART_DRV_MAX_INSTANCES) {
        s_drv_table[s_drv_count++] = drv;
    } else {
        DBG_LOG("UART DRV: max instances reached!");
    }
}

static uart_drv_t* uart_drv_find(UART_HandleTypeDef *huart) {
    for (int i = 0; i < s_drv_count; i++) {
        if (s_drv_table[i]->huart == huart) {
            return s_drv_table[i];
        }
    }
    return NULL;
}

/* ===========================
 * RS485 控制
 * =========================== */
static void rs485_tx_enable(uart_drv_t *drv) {
    if (drv->rs485 == NULL) return;
    HAL_GPIO_WritePin(drv->rs485->port, drv->rs485->pin,
                      drv->rs485->active_level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void rs485_rx_enable(uart_drv_t *drv) {
    if (drv->rs485 == NULL) return;
    HAL_GPIO_WritePin(drv->rs485->port, drv->rs485->pin,
                      drv->rs485->active_level ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* ===========================
 * ★ 核心修复：安全的接收启动
 * =========================== */
static void start_rx(uart_drv_t *drv) {
    if (drv == NULL) return;
    if (drv->state == UART_DRV_ERROR) return;

    UART_HandleTypeDef *huart = drv->huart;
    
    /* ★ 修复问题4：不再暴力 AbortReceive，而是优雅地停止 DMA */
    if (huart->hdmarx != NULL) {
        /* 如果 DMA 正在运行，先停止 */
        if (HAL_DMA_GetState(huart->hdmarx) == HAL_DMA_STATE_BUSY) {
            HAL_DMA_Abort(huart->hdmarx);
        }
        
        /* 重新配置 DMA 并启动 */
        HAL_DMA_Start_IT(huart->hdmarx,
                         (uint32_t)&huart->Instance->RDR,
                         (uint32_t)drv->rx_buf,
                         UART_DRV_BUF_SIZE);
        
        /* 使能 DMA 接收 */
        SET_BIT(huart->Instance->CR3, USART_CR3_DMAR);
    }
    
    /* 清除所有标志位（保留已接收的数据） */
    __HAL_UART_CLEAR_FLAG(huart, UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    
    /* 使能空闲中断 */
    SET_BIT(huart->Instance->CR1, USART_CR1_IDLEIE);
    
    drv->rx_len = 0;
    drv->state = UART_DRV_IDLE;
}

/* ===========================
 * API 实现
 * =========================== */

void uart_drv_init(uart_drv_t *drv, UART_HandleTypeDef *huart, uart_rs485_t *rs485) {
    memset(drv, 0, sizeof(uart_drv_t));
    drv->huart = huart;
    drv->rs485 = rs485;
    drv->state = UART_DRV_IDLE;
    drv->locked = 0;
    drv->rx_overrun_cnt = 0;
    drv->rx_dropped_bytes = 0;
    
    drv->cfg.baudrate = huart->Init.BaudRate;
    drv->cfg.word_length = huart->Init.WordLength;
    drv->cfg.stop_bits = huart->Init.StopBits;
    drv->cfg.parity = huart->Init.Parity;
    
    uart_drv_register_instance(drv);
    
    rs485_rx_enable(drv);
    start_rx(drv);
    
    DBG_LOG("UART DRV init: %s", uart_drv_get_name(huart));
}

int uart_drv_reconfig(uart_drv_t *drv, const uart_drv_cfg_t *cfg) {
    if (drv == NULL || cfg == NULL) return -1;
    
    UART_HandleTypeDef *huart = drv->huart;
    
    /* 停止所有正在进行的传输 */
    HAL_UART_DMAStop(huart);
    HAL_UART_Abort(huart);
    HAL_UART_DeInit(huart);
    
    /* 重新配置 */
    huart->Init.BaudRate = cfg->baudrate;
    huart->Init.WordLength = cfg->word_length;
    huart->Init.StopBits = cfg->stop_bits;
    huart->Init.Parity = cfg->parity;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    
    if (HAL_UART_Init(huart) != HAL_OK) {
        drv->state = UART_DRV_ERROR;
        if (drv->on_error) drv->on_error(drv);
        return -2;
    }
    
    drv->cfg = *cfg;
    drv->state = UART_DRV_IDLE;
    drv->locked = 0;
    drv->rx_len = 0;
    drv->rx_overrun_cnt = 0;
    drv->rx_dropped_bytes = 0;
    
    rs485_rx_enable(drv);
    start_rx(drv);
    return 0;
}

void uart_drv_reg_cb(uart_drv_t *drv,
                     void (*on_recv)(uart_drv_t *, uint8_t *, uint16_t),
                     void (*on_sent)(uart_drv_t *),
                     void (*on_error)(uart_drv_t *)) {
    if (drv == NULL) return;
    if (on_recv) drv->on_recv = on_recv;
    if (on_sent) drv->on_sent = on_sent;
    if (on_error) drv->on_error = on_error;
}

int uart_drv_send(uart_drv_t *drv, const uint8_t *data, uint16_t len) {
    if (drv == NULL || data == NULL || len == 0) return -1;
    if (drv->state == UART_DRV_TX_BUSY) return -2;
    if (drv->state == UART_DRV_DATA_READY && drv->locked) return -3;
    
    /* ★ 修复问题3：发送期间若已收到数据，记录但不丢弃（已在 on_idle 中处理） */
    /* 停止接收 DMA（但保留已接收的数据） */
    if (drv->huart->hdmarx != NULL) {
        HAL_DMA_Abort(drv->huart->hdmarx);
    }
    CLEAR_BIT(drv->huart->Instance->CR3, USART_CR3_DMAR);
    CLEAR_BIT(drv->huart->Instance->CR1, USART_CR1_IDLEIE);
    
    /* 复制数据到发送缓冲 */
    memcpy(drv->tx_buf, data, len);
    
    rs485_tx_enable(drv);
    drv->state = UART_DRV_TX_BUSY;
    
    if (drv->huart->hdmatx != NULL) {
        HAL_UART_Transmit_DMA(drv->huart, drv->tx_buf, len);
    } else {
        HAL_UART_Transmit_IT(drv->huart, drv->tx_buf, len);
    }
    return 0;
}

/* ★ 修复问题2：get_packet 增加空包检查 */
uint8_t* uart_drv_get_packet(uart_drv_t *drv, uint16_t *len) {
    if (drv == NULL || len == NULL) return NULL;
    if (drv->state != UART_DRV_DATA_READY) return NULL;
    if (drv->locked) return NULL;
    if (drv->rx_len == 0) return NULL;   /* ★ 防止空包 */
    
    drv->locked = 1;
    *len = drv->rx_len;
    return drv->rx_buf;
}

/* ★ 修复问题1：release_packet 必须检查 locked */
void uart_drv_release_packet(uart_drv_t *drv) {
    if (drv == NULL) return;
    if (drv->state != UART_DRV_DATA_READY) return;
    if (!drv->locked) {
        /* ★ 没被锁定说明上层没有调用 get_packet，不能释放 */
        DBG_LOG("UART DRV: release without lock! pin=%s", uart_drv_get_name(drv->huart));
        return;
    }
    
    drv->locked = 0;
    drv->rx_len = 0;
    drv->state = UART_DRV_IDLE;
    
    /* 重新启动接收 */
    start_rx(drv);
}

/* ★ 修复问题2：available 检查 locked 状态 */
uint16_t uart_drv_available(uart_drv_t *drv) {
    if (drv == NULL) return 0;
    if (drv->state == UART_DRV_DATA_READY && drv->locked == 0) {
        return drv->rx_len;
    }
    return 0;
}

uart_drv_state_t uart_drv_get_state(uart_drv_t *drv) {
    return drv->state;
}

void uart_drv_get_stats(uart_drv_t *drv, uint32_t *out_overrun, uint32_t *out_dropped) {
    if (drv == NULL) return;
    if (out_overrun) *out_overrun = drv->rx_overrun_cnt;
    if (out_dropped) *out_dropped = drv->rx_dropped_bytes;
}

void uart_drv_reset(uart_drv_t *drv) {
    if (drv == NULL) return;
    
    HAL_UART_DMAStop(drv->huart);
    __HAL_UART_CLEAR_FLAG(drv->huart, UART_FLAG_ORE | UART_FLAG_FE | UART_FLAG_NE);
    
    drv->state = UART_DRV_IDLE;
    drv->locked = 0;
    drv->rx_len = 0;
    drv->rx_overrun_cnt = 0;
    drv->rx_dropped_bytes = 0;
    
    rs485_rx_enable(drv);
    start_rx(drv);
}

void uart_drv_get_cfg(uart_drv_t *drv, uart_drv_cfg_t *cfg) {
    if (drv && cfg) *cfg = drv->cfg;
}

/* ===========================
 * ★ 中断入口（全部修复）
 * =========================== */

void uart_drv_on_tx_done(UART_HandleTypeDef *huart) {
    uart_drv_t *drv = uart_drv_find(huart);
    if (drv == NULL || drv->state != UART_DRV_TX_BUSY) return;
    
    drv->state = UART_DRV_IDLE;
    rs485_rx_enable(drv);
    
    /* 发送完成回调 */
    if (drv->on_sent) drv->on_sent(drv);
    
    /* 重新启动接收（使用安全的 start_rx） */
    start_rx(drv);
}

void uart_drv_on_idle(UART_HandleTypeDef *huart, uint16_t size) {
    uart_drv_t *drv = uart_drv_find(huart);
    if (drv == NULL) return;
    
    /* ★ 修复问题3：发送期间收到数据 -> 记录丢包 */
    if (drv->state == UART_DRV_TX_BUSY) {
        drv->rx_overrun_cnt++;
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        return;
    }
    
    /* ★ 如果已有数据未处理，忽略新数据 */
    if (drv->state == UART_DRV_DATA_READY) {
        drv->rx_overrun_cnt++;
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        return;
    }
    
    /* ★ 如果被锁定，忽略 */
    if (drv->locked) {
        drv->rx_overrun_cnt++;
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        return;
    }

    /* ★★★ 关键修复：使用 HAL 传入的 size 参数，而不是读取 DMA 计数器 ★★★ */
    HAL_UART_RxEventTypeTypeDef rx_evt = HAL_UARTEx_GetRxEventType(huart);
    if (rx_evt != HAL_UART_RXEVENT_IDLE && rx_evt != HAL_UART_RXEVENT_TC) {
        /* HT 等中间事件：不要停止 DMA */
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        return;
    }

    /* ★ 使用 HAL 传来的 size，不再依赖 DMA 计数器 */
    if (size > 0) {
        drv->rx_len = size;
        
        /* 停止 DMA 接收（防止后续数据干扰） */
        HAL_UART_DMAStop(drv->huart);
        
        drv->state = UART_DRV_DATA_READY;
        drv->locked = 1;   /* 锁定，等待上层处理 */
        
        /* 回调通知（如果注册了） */
        if (drv->on_recv) {
            drv->on_recv(drv, drv->rx_buf, drv->rx_len);
        }
    }
    
    __HAL_UART_CLEAR_IDLEFLAG(huart);
}

void uart_drv_on_error(UART_HandleTypeDef *huart) {
    uart_drv_t *drv = uart_drv_find(huart);
    if (drv == NULL) return;
    
    drv->state = UART_DRV_ERROR;
    drv->rx_dropped_bytes += drv->rx_len;  /* 记录丢失数据 */
    
    if (drv->on_error) drv->on_error(drv);
    
    /* 自动恢复 */
    uart_drv_reset(drv);
}