#include "task_wol.h"
#include "wol.h"
#include "hlk_rm58s_port.h"      // ← 添加，获取 HLK_LOG, HLK_GET_TICK, HLK_Delay_ms
#include "network_config.h"
#include "cJSON.h"

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

#include <string.h>
#include <stdlib.h>

// ===========================
// WOL 日志 - 直接使用 HLK_LOG，避免重复定义
// ===========================
#undef WOL_LOG
#define WOL_LOG(fmt, ...)  HLK_LOG("[WOL] " fmt, ##__VA_ARGS__)

// ===========================
// 内部状态
// ===========================
typedef struct {
    wol_state_t state;
#if HLK_USE_RTOS
    TaskHandle_t task_handle;      // ← 只在 RTOS 启用时使用
#endif
    bool initialized;
    bool send_request;
    uint32_t last_send_tick;
    uint8_t retry_count;
} wol_task_context_t;

static wol_task_context_t g_ctx = {0};

// ===========================
// 函数声明
// ===========================
static void wol_task_routine(void *arg);
static void wol_task_send_internal(void);

// ===========================
// WOL 任务主循环
// ===========================
static void wol_task_routine(void *arg) {
    (void)arg;
    
    WOL_LOG("WOL Task Started\n");
    
    g_ctx.state = WOL_STATE_IDLE;
    
    while (1) {
        if (g_ctx.send_request) {
            g_ctx.send_request = false;
            wol_task_send_internal();
        }
        
        if (g_ctx.state == WOL_STATE_FAILED && g_ctx.retry_count < 3) {
            uint32_t now = HLK_GET_TICK();
            if (now - g_ctx.last_send_tick > 30000) {
                WOL_LOG("Retry sending WOL (%d/3)\n", g_ctx.retry_count + 1);
                wol_task_send_internal();
            }
        }
        
        HLK_Delay_ms(100);
    }
}

// ===========================
// 发送 WOL (内部)
// ===========================
static void wol_task_send_internal(void) {
    WOL_LOG("Sending WOL...\n");
    g_ctx.state = WOL_STATE_SENDING;
    g_ctx.last_send_tick = HLK_GET_TICK();
    
    err_t result = send_wol();
    
    if (result == ERR_OK) {
        WOL_LOG("WOL sent successfully\n");
        g_ctx.state = WOL_STATE_SUCCESS;
        g_ctx.retry_count = 0;
    } else {
        WOL_LOG("WOL failed (err=%d)\n", result);
        g_ctx.state = WOL_STATE_FAILED;
        g_ctx.retry_count++;
        if (g_ctx.retry_count >= 3) {
            WOL_LOG("WOL failed 3 times, stop retry\n");
        }
    }
}

// ===========================
// 对外 API
// ===========================
void WOL_Task_Init(void) {
    if (g_ctx.initialized) return;
    
    WOL_LOG("Initializing...\n");
    g_ctx.initialized = true;
    
#if HLK_USE_RTOS
    BaseType_t ret = xTaskCreate(wol_task_routine, 
                                 "WOLTask", 
                                 WOL_TASK_STACK_SIZE, 
                                 NULL, 
                                 WOL_TASK_PRIORITY, 
                                 &g_ctx.task_handle);
    if (ret == pdPASS) {
        WOL_LOG("Task created\n");
    } else {
        WOL_LOG("Task creation FAILED!\n");
    }
#else
    wol_task_routine(NULL);
#endif
}

void WOL_Task_Send(void) {
    g_ctx.send_request = true;
    WOL_LOG("Send request queued\n");
}

wol_state_t WOL_Task_GetState(void) {
    return g_ctx.state;
}

bool WOL_Task_IsSending(void) {
    return (g_ctx.state == WOL_STATE_SENDING);
}

// ===========================
// MQTT 控制接口
// ===========================
void WOL_Task_OnCommand(const char *cmd, const char *params_json) {
    if (!cmd) return;
    
    WOL_LOG("Command: %s\n", cmd);
    
    if (strcmp(cmd, "wol") == 0) {
        if (params_json) {
            cJSON *root = cJSON_Parse(params_json);
            if (root) {
                cJSON_Delete(root);
            }
        }
        WOL_Task_Send();
    } else {
        WOL_LOG("Unknown command: %s\n", cmd);
    }
}