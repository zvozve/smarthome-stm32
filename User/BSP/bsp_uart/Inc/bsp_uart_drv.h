/**
 * @file    bsp_uart_drv.h
 * @brief   通用串口DMA驱动 - 状态机管理（修复版）
 * @version V3.1
 * @date    2026-08-24
 * 
 * @note    功能说明：
 *          1. 基于DMA+空闲中断的串口数据收发
 *          2. 内置状态机管理：IDLE / TX_BUSY / DATA_READY / ERROR
 *          3. 支持RS485发送使能脚管理（由上层注入GPIO）
 *          4. 支持运行时波特率动态重配置
 *          5. 数据包锁定机制，防止接收数据被覆盖 ★修复
 *          6. 支持发送期间接收数据计数 ★新增
 *          7. 完全硬件无关，由上层注入 huart 句柄
 * 
 * @note    使用流程（推荐）：
 *          1. 上层分配 uart_drv_t 实例
 *          2. 调用 uart_drv_init() 注入 huart 和 RS485 配置
 *          3. 调用 uart_drv_reg_cb() 注册回调（可选）
 *          4. 中断中调用 uart_drv_on_tx_done() / uart_drv_on_idle() / uart_drv_on_error()
 *          5. 主循环：uart_drv_available() -> uart_drv_get_packet() -> 处理 -> uart_drv_release_packet()
 */

#ifndef __UART_DRV_H__
#define __UART_DRV_H__

#include <stdint.h>
#include <stdbool.h>
#include "hal_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART_DRV_BUF_SIZE   256
#define UART_DRV_MAX_INSTANCES  4   /* ★ 明确最大实例数 */

/* ===========================
 * 状态机
 * =========================== */
typedef enum {
    UART_DRV_IDLE = 0,      /* 空闲，可以收发 */
    UART_DRV_TX_BUSY,       /* 发送中 */
    UART_DRV_DATA_READY,    /* 数据已就绪，等待上层读取 */
    UART_DRV_ERROR          /* 错误状态 */
} uart_drv_state_t;

/* ===========================
 * 串口参数
 * =========================== */
typedef struct {
    uint32_t baudrate;
    uint8_t  word_length;
    uint8_t  stop_bits;
    uint8_t  parity;
} uart_drv_cfg_t;

/* ===========================
 * RS485配置
 * =========================== */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       active_level;  /* 1=高电平发送, 0=低电平发送 */
} uart_rs485_t;

/* ===========================
 * 驱动实例
 * =========================== */
typedef struct uart_drv_instance {
    /* 硬件句柄 */
    UART_HandleTypeDef *huart;
    uart_rs485_t *rs485;
    
    /* 缓冲区 */
    uint8_t rx_buf[UART_DRV_BUF_SIZE];
    uint16_t rx_len;
    uint8_t tx_buf[UART_DRV_BUF_SIZE];
    
    /* ★ 状态管理 ★ */
    volatile uart_drv_state_t state;
    uint8_t locked;              /* 仅当 state=DATA_READY 时有效：1=已被上层取走 */
    
    /* ★ 新增：丢包统计 ★ */
    volatile uint32_t rx_overrun_cnt;   /* 发送期间/缓冲满时丢失的数据包计数 */
    volatile uint32_t rx_dropped_bytes; /* 因状态错误丢弃的字节数 */
    
    /* 配置 */
    uart_drv_cfg_t cfg;
    
    /* 回调（可选，推荐使用主循环轮询模式） */
    void (*on_recv)(struct uart_drv_instance *pInst, uint8_t *data, uint16_t len);
    void (*on_sent)(struct uart_drv_instance *pInst);
    void (*on_error)(struct uart_drv_instance *pInst);
} uart_drv_t;

/* ===========================
 * API
 * =========================== */

/**
 * @brief   初始化串口驱动
 * @param   drv     驱动实例指针
 * @param   huart   HAL UART 句柄
 * @param   rs485   RS485 配置（NULL 表示不使用）
 */
void uart_drv_init(uart_drv_t *drv, UART_HandleTypeDef *huart, uart_rs485_t *rs485);

/**
 * @brief   动态重配置波特率/参数
 * @param   drv     驱动实例指针
 * @param   cfg     新配置
 * @retval  0: 成功, -1: 参数错误, -2: HAL初始化失败
 */
int uart_drv_reconfig(uart_drv_t *drv, const uart_drv_cfg_t *cfg);

/**
 * @brief   注册回调函数（可选，推荐使用主循环轮询）
 */
void uart_drv_reg_cb(uart_drv_t *drv,
                     void (*on_recv)(uart_drv_t *, uint8_t *, uint16_t),
                     void (*on_sent)(uart_drv_t *),
                     void (*on_error)(uart_drv_t *));

/**
 * @brief   发送数据（阻塞式等待发送完成）
 * @param   drv     驱动实例指针
 * @param   data    数据指针
 * @param   len     数据长度
 * @retval  0: 成功, -1: 参数错误, -2: 发送忙, -3: 数据被锁定
 */
int uart_drv_send(uart_drv_t *drv, const uint8_t *data, uint16_t len);

/**
 * @brief   获取接收数据包（必须配对调用 release_packet）
 * @param   drv     驱动实例指针
 * @param   len     输出：数据长度
 * @retval  数据指针，NULL 表示无数据或已被锁定
 */
uint8_t* uart_drv_get_packet(uart_drv_t *drv, uint16_t *len);

/**
 * @brief   释放数据包（必须配对调用 get_packet）
 * @param   drv     驱动实例指针
 */
void uart_drv_release_packet(uart_drv_t *drv);

/**
 * @brief   查询可用数据长度（非锁定，用于主循环轮询）
 * @param   drv     驱动实例指针
 * @retval  可读数据长度，0 表示无数据
 */
uint16_t uart_drv_available(uart_drv_t *drv);

/**
 * @brief   获取当前状态
 */
uart_drv_state_t uart_drv_get_state(uart_drv_t *drv);

/**
 * @brief   获取丢包统计
 */
void uart_drv_get_stats(uart_drv_t *drv, uint32_t *out_overrun, uint32_t *out_dropped);

/**
 * @brief   重置驱动
 */
void uart_drv_reset(uart_drv_t *drv);

/**
 * @brief   获取配置
 */
void uart_drv_get_cfg(uart_drv_t *drv, uart_drv_cfg_t *cfg);

/**
 * @brief   工具接口
 */
const char* uart_drv_get_name(UART_HandleTypeDef *huart);
uint8_t uart_drv_get_index(UART_HandleTypeDef *huart);

/* ===========================
 * 中断入口（由 HAL 回调转发）
 * =========================== */
void uart_drv_on_tx_done(UART_HandleTypeDef *huart);
void uart_drv_on_idle(UART_HandleTypeDef *huart, uint16_t size);
void uart_drv_on_error(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __UART_DRV_H__ */