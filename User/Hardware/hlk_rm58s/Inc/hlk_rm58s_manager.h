#ifndef __HLK_RM58S_MANAGER_H__
#define __HLK_RM58S_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>
#include "hlk_rm58s_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HLK_LINK_RERTY       (30)      // 连接重试次数

// ===========================
// 管理器状态（内部使用）
// ===========================
typedef enum {
    MGR_IDLE = 0,
    MGR_CONFIGURING,
    MGR_WAIT_WIFI,
    MGR_WAIT_SOCKET,
    MGR_READY,
    MGR_RECONFIG,
} mgr_status_t;

// ===========================
// 回调类型
// ===========================
typedef void (*mgr_rx_callback_t)(const char* data, uint16_t len);

// ===========================
// Manager API（仅供Client调用）
// ===========================
void HLK_Manager_Init(void);
void HLK_Manager_Task(void);
void HLK_Manager_Reconfig(void);

mgr_status_t HLK_Manager_GetStatus(void);
bool HLK_Manager_IsReady(void);

void HLK_Manager_RegisterRxCallback(mgr_rx_callback_t cb);

// 驱动层回调（由Driver调用）
void HLK_Manager_OnResponseComplete(void);
void HLK_Manager_OnTimeout(void);
void HLK_Manager_OnError(void);

#ifdef __cplusplus
}
#endif

#endif