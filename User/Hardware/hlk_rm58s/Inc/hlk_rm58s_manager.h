#ifndef __HLK_RM58S_MANAGER_H__
#define __HLK_RM58S_MANAGER_H__

#include <stdint.h>
#include <stdbool.h>

// ===========================
// 状态枚举
// ===========================
typedef enum {
    HLK_MGR_IDLE = 0,
    HLK_MGR_CONFIGURING,
    HLK_MGR_WAIT_WIFI,
    HLK_MGR_WAIT_SOCKET,
    HLK_MGR_READY,
    HLK_MGR_RECONFIG,
    HLK_MGR_OFFLINE,
    HLK_MGR_ERROR,
} hlk_mgr_status_t;

// ===========================
// 回调
// ===========================
typedef void (*mgr_rx_callback_t)(const uint8_t* data, uint16_t len);
typedef void (*mgr_status_callback_t)(hlk_mgr_status_t status);

// ===========================
// 对外接口（只有一个初始化函数）
// ===========================
void HLK_Manager_Init(void);

#endif