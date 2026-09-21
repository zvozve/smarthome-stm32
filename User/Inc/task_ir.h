/**
 * @file    task_ir.h
 * @brief   红外控制任务（接收：SDK ir_receiver / 发射：SDK ir_transmitter）
 * @version V2.0
 * @date    2026-09-21
 */

#ifndef __TASK_IR_H
#define __TASK_IR_H

#include <stdint.h>
#include <stdbool.h>

#include "board_cfg.h"      /* 工程绑定（含 BOARD_IR_RX_BUF_SIZE / IR 定时器宏） */
#include "cmsis_os.h"
#include "ir_receiver.h"    /* 新 SDK 接收驱动 */
#include "ir_transmitter.h" /* 新 SDK 发射驱动 */
#include "ac_codec.h"       /* 空调编码 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 数据结构 ========== */

/* 最近一次接收到的原始帧：直接保存 SDK 接收驱动产出的时长序列
 * （偶数下标 = mark/有载波，奇数下标 = space）。缓冲大小由 board_cfg 的
 * BOARD_IR_RX_BUF_SIZE 决定。 */
typedef struct {
    uint16_t        timings[BOARD_IR_RX_BUF_SIZE];
    uint16_t        count;
    bool            has_new;        /* 是否有新数据 */
    uint32_t        timestamp;      /* 接收时间戳 */
} IR_RecvData_t;

/* ========== 任务配置 ========== */

#define IR_TASK_NAME            "IRTask"
#define IR_TASK_PERIOD_MS       20          /* 接收轮询周期 */
#define IR_TASK_PRIORITY        osPriorityNormal
/* 栈说明：IR_RecvData_t 含 256 条时长（512B）+ 局部拷贝，512 字(2KB)偏紧，提到 1024 字(4KB) */
#define IR_TASK_STACK_SIZE      1024

/* ========== API ========== */

/**
 * @brief  创建红外任务
 */
bool Task_IR_Init(void);

/**
 * @brief  获取最近一次接收到的原始帧（线程安全）
 * @param  pData  输出
 * @retval true 有新数据并已复制
 */
bool Task_IR_GetRecvData(IR_RecvData_t *pData);

/**
 * @brief  清除“有新数据”标志
 */
void Task_IR_ClearNewFlag(void);

/**
 * @brief  预留：根据 MQTT 指令发送红外
 */
bool Task_IR_SendByMQTT(const uint8_t *cmd, uint16_t cmd_len);

/**
 * @brief  发送空调控制指令（语义状态 → 协议编码 → 载波发射）
 * @retval true 发送完成
 */
bool Task_IR_SendAc(ac_brand_t brand, const ac_state_t *state);

/**
 * @brief  由 Core 的 HAL_TIM_IC_CaptureCallback 调用，转发到 SDK 接收驱动 ISR
 */
void task_ir_rx_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_IR_H */
