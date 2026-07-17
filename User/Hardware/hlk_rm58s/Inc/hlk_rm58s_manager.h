#ifndef __HLK_RM58S_MANAGER_H__
#define __HLK_RM58S_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>

// ===========================
// 状态枚举
// ===========================
typedef enum {
    HLK_MGR_DISCONNECTED = 0,   // 未连接
    HLK_MGR_CONNECTED,          // 已连接
    HLK_MGR_CONFIGURING,        // 配置中
} hlk_mgr_status_t;

// ===========================
// 回调
// ===========================
typedef void (*mgr_rx_callback_t)(const uint8_t* data, uint16_t len);
typedef void (*mgr_status_callback_t)(hlk_mgr_status_t status);

// ===========================
// 回调注册接口（新增）
// ===========================
int HLK_Manager_RegisterRxCallback(mgr_rx_callback_t callback);
int HLK_Manager_UnregisterRxCallback(mgr_rx_callback_t callback);
void HLK_Manager_RegisterStatusCallback(mgr_status_callback_t callback);

// ===========================
// 对外接口
// ===========================
void HLK_Manager_Init(void);
hlk_mgr_status_t HLK_Manager_GetStatus(void);

#endif