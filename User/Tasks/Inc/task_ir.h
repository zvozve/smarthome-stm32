/**
 * @file    task_ir.h
 * @brief   红外控制任务（接收 + MQTT发送预留）
 * @version V1.0
 * @date    2026-08-25
 */

#ifndef __TASK_IR_H
#define __TASK_IR_H

#include "main.h"
#include "cmsis_os.h"
#include "ir_1838b.h"
#include "ac_codec.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 数据结构 ========== */

typedef struct {
    ir_raw_frame_t  raw;            /* 最近一次接收到的原始帧 */
    bool            has_new;        /* 是否有新数据 */
    uint32_t        timestamp;      /* 接收时间戳 */
} IR_RecvData_t;

/* ========== 任务配置 ========== */

#define IR_TASK_NAME            "IRTask"
#define IR_TASK_PERIOD_MS       20          /* 接收轮询周期 */
#define IR_TASK_PRIORITY        osPriorityNormal
/* 栈说明：ir_raw_frame_t 扩到 320 边沿后约 650B，任务内有一份局部拷贝，
 * 再叠加日志/调用开销，512 字(2KB)偏紧，提到 1024 字(4KB) */
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
 * @param  cmd      指令字符串或编码
 * @param  cmd_len  长度
 * @retval true 已接受发送请求
 * @note   当前仅预留接口，内部暂未实现发送
 */
bool Task_IR_SendByMQTT(const uint8_t *cmd, uint16_t cmd_len);

/**
 * @brief  发送空调控制指令（语义状态 → 协议编码 → 载波发射）
 * @param  brand  品牌（AC_BRAND_MIDEA / AC_BRAND_CHIGO）
 * @param  state  空调状态（电源/模式/温度/风速/强力）
 * @retval true 发送完成
 * @note   阻塞约 100~200ms（一帧编码时长），请在任务上下文调用
 */
bool Task_IR_SendAc(ac_brand_t brand, const ac_state_t *state);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_IR_H */