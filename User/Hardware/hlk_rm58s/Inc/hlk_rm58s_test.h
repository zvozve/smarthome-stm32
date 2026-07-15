#ifndef __HLK_RM58S_TEST_H__
#define __HLK_RM58S_TEST_H__

#include <stdint.h>
#include <stdbool.h>
#include "hlk_rm58s_port.h"  // 包含 HLK_USE_RTOS 定义

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// 通用接口（裸机和RTOS共用）
// ===========================
void HLK_Test_Init(void);
void HLK_Test_Task(void);   // 裸机：周期性调用 | RTOS：空函数

// ===========================
// RTOS专用接口（仅在RTOS模式下可用）
// ===========================
#if HLK_USE_RTOS
void HLK_Test_RTOS_Reconnect(void);
bool HLK_Test_RTOS_Send(const char* data);
void HLK_Test_RTOS_GetStats(uint32_t* send, uint32_t* recv, uint32_t* err, uint32_t* recon);
void HLK_Test_RTOS_ResetStats(void);
TaskHandle_t HLK_Test_RTOS_GetTaskHandle(void);
#endif

#ifdef __cplusplus
}
#endif

#endif