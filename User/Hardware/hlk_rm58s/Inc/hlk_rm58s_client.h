#ifndef __HLK_RM58S_CLIENT_H__
#define __HLK_RM58S_CLIENT_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>          // 添加：for size_t

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// 状态枚举（对外）
// ===========================
typedef enum {
    HLK_IDLE = 0,
    HLK_CONFIGURING,
    HLK_WAIT_WIFI,
    HLK_WAIT_SOCKET,
    HLK_READY,
    HLK_RECONFIG,
} hlk_status_t;

// ===========================
// 回调类型定义
// ===========================
typedef void (*hlk_rx_callback_t)(const char* data, uint16_t len);
typedef void (*hlk_status_callback_t)(hlk_status_t status);

// ===========================
// Client API（唯一对外接口）
// ===========================

// 初始化
void HLK_Client_Init(void);

// 任务循环（周期性调用，裸机用）
void HLK_Client_Task(void);

// 数据发送
bool HLK_Client_Send(const char* data, uint16_t len);
bool HLK_Client_SendString(const char* str);

// 状态查询
hlk_status_t HLK_Client_GetStatus(void);
bool HLK_Client_IsReady(void);

// 回调注册
void HLK_Client_SetRxCallback(hlk_rx_callback_t cb);
void HLK_Client_SetStatusCallback(hlk_status_callback_t cb);

// 控制
void HLK_Client_Reconnect(void);

// JSON工具（提供给用户使用）
void HLK_Client_BuildJSON(char* buf, size_t buf_size);
void HLK_Client_ParseJSON(char *json);

#ifdef __cplusplus
}
#endif

#endif