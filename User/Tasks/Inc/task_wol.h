#ifndef __TASK_WOL_H__
#define __TASK_WOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// WOL 任务配置
// ===========================
#define WOL_TASK_STACK_SIZE     512
#define WOL_TASK_PRIORITY       osPriorityNormal

// ===========================
// WOL 状态
// ===========================
typedef enum {
    WOL_STATE_IDLE = 0,
    WOL_STATE_SENDING,
    WOL_STATE_SUCCESS,
    WOL_STATE_FAILED,
} wol_state_t;

// ===========================
// API
// ===========================
void WOL_Task_Init(void);
void WOL_Task_Send(void);
wol_state_t WOL_Task_GetState(void);
bool WOL_Task_IsSending(void);

// MQTT 控制接口 (由 app_main 调用)
void WOL_Task_OnCommand(const char *cmd, const char *params_json);

#ifdef __cplusplus
}
#endif

#endif /* __TASK_WOL_H__ */