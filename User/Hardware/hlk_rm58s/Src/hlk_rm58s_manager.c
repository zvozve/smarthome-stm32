#include "hlk_rm58s_manager.h"
#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_port.h"
#include "hlk_rm58s_config.h"
#include <string.h>
#include <ctype.h>

#if HLK_USE_RTOS
#include "FreeRTOS.h"
#include "task.h"
#endif

// ===========================
// 配置
// ===========================
#define HLK_LINK_RERTY      30
#define HLK_CMD_TIMEOUT_MS  3000
#define HLK_MAX_RETRY       3
#define HLK_POWER_ON_WAIT   5000
#define HLK_RECONFIG_WAIT   10000

// ===========================
// AT指令项
// ===========================
typedef struct {
    const char *cmd;
    const char *expect;      // NULL=不检查
    uint32_t timeout_ms;
    uint8_t max_retry;
} at_cmd_t;

// ===========================
// AT指令表（使用config.h中的宏）
// ===========================
static const at_cmd_t wifi_cmds[] = {
    {AT_NETMODE_STA,            "ok", 3000, 3},
    {AT_NETMODE_STA,            "ok", 3000, 3},
    {AT_WIFI_CONF,              "ok", 5000, 3},
    {AT_DHCPC_ON,               "ok", 3000, 3},
    {AT_REMOTEIP,               "ok", 3000, 3},
    {AT_REMOTEPORT,             "ok", 3000, 3},
    {AT_QUERY_MAC,              "Get_MAC", 3000, 2},
    {AT_REMOTEPRO,              NULL, 2000, 2},
    {AT_MODE_CLIENT,            NULL, 2000, 2},
    {AT_UART,                   "ok", 3000, 3},
    {AT_UARTPACKLEN,            "ok", 3000, 3},
    {AT_UARTPACKTIMEOUT,        "ok", 3000, 3},
    {AT_NET_COMMIT,             "ok", 5000, 5},
    {AT_RECONN,                 NULL, 2000, 2},
};

#define CMD_COUNT (sizeof(wifi_cmds) / sizeof(at_cmd_t))

// ===========================
// 上下文
// ===========================
static struct {
    hlk_mgr_status_t status;
    bool is_first_connect;
    
    // AT配置
    uint8_t cmd_index;
    uint8_t cmd_retry;
    uint32_t cmd_start_tick;
    bool waiting_response;
    char response[256];
    uint16_t response_len;
    
    // 连接检测
    uint8_t wifi_retry;
    uint8_t socket_retry;
    uint32_t last_check_tick;
    uint32_t wait_start_tick;
    bool is_waiting_stable;
    
    // 错误
    uint8_t uart_error_count;
    uint8_t reconnect_fail_count;
    
    // 回调
    mgr_rx_callback_t rx_callback;
    mgr_status_callback_t status_callback;
    
    // 任务
    TaskHandle_t task_handle;
    bool initialized;
} mgr = {0};

// ===========================
// 函数声明
// ===========================
static void mgr_task(void *arg);
static void mgr_start_reconfig(void);
static void mgr_send_next_cmd(void);
static void mgr_handle_response(void);
static void mgr_check_connection(void);
static void mgr_wait_stable(void);
static void mgr_update_status(hlk_mgr_status_t status);
static bool mgr_parse_response(const char *resp, const char *expect);
static const char* mgr_status_str(hlk_mgr_status_t s);

// ===========================
// 驱动回调
// ===========================
static void on_rx(uint8_t *data, uint16_t len) {
    if (len == 0 || data == NULL) return;
    
    // 打印HEX
    HLK_LOG("on_rx: %s\n", data);

    if (mgr.status == HLK_MGR_READY) {
        if (mgr.rx_callback) {
            mgr.rx_callback(data, len);
        }
        return;
    }
    
    if (mgr.waiting_response) {
        uint16_t copy_len = len > sizeof(mgr.response) - 1 ? sizeof(mgr.response) - 1 : len;
        memcpy(mgr.response, data, copy_len);
        mgr.response[copy_len] = '\0';
        mgr.response_len = copy_len;
        
        // ⭐ 关键：标记响应已收到
        mgr.waiting_response = false;
        
        HLK_LOG("on_rx: response saved: %s\n", mgr.response);
    } else {
        HLK_LOG("on_rx: not waiting for response, ignored\n");
    }
}

static void on_tx(void) {
    HLK_LOG("on_tx: TX complete\n");
}

static void on_err(void) {
    mgr.uart_error_count++;
    HLK_LOG("on_err: UART error count=%d\n", mgr.uart_error_count);
    if (mgr.uart_error_count >= 3) {
        HLK_LOG("on_err: Too many UART errors, reconfig\n");
        mgr.uart_error_count = 0;
        mgr_start_reconfig();
    }
}

// ===========================
// 状态工具
// ===========================
static const char* mgr_status_str(hlk_mgr_status_t s) {
    switch(s) {
        case HLK_MGR_IDLE:       return "IDLE";
        case HLK_MGR_CONFIGURING:return "CONFIGURING";
        case HLK_MGR_WAIT_WIFI:  return "WAIT_WIFI";
        case HLK_MGR_WAIT_SOCKET:return "WAIT_SOCKET";
        case HLK_MGR_READY:      return "READY";
        case HLK_MGR_RECONFIG:   return "RECONFIG";
        case HLK_MGR_OFFLINE:    return "OFFLINE";
        case HLK_MGR_ERROR:      return "ERROR";
        default:                 return "UNKNOWN";
    }
}

static void mgr_update_status(hlk_mgr_status_t status) {
    if (mgr.status != status) {
        HLK_LOG("mgr_update_status: %s -> %s\n", mgr_status_str(mgr.status), mgr_status_str(status));
        mgr.status = status;
        if (mgr.status_callback) {
            mgr.status_callback(status);
        }
    }
}

// ===========================
// 响应解析（不区分大小写）
// ===========================
static bool mgr_parse_response(const char *resp, const char *expect) {
    if (!resp || strlen(resp) == 0) {
        HLK_LOG("mgr_parse_response: empty response\n");
        return false;
    }
    if (expect == NULL) {
        HLK_LOG("mgr_parse_response: no expect, return true\n");
        return true;
    }
    
    HLK_LOG("mgr_parse_response: looking for '%s' in '%s'\n", expect, resp);
    
    const char *p = resp;
    while (*p) {
        const char *p2 = p;
        const char *e2 = expect;
        while (*p2 && *e2 && tolower(*p2) == tolower(*e2)) {
            p2++; e2++;
        }
        if (!*e2) {
            HLK_LOG("mgr_parse_response: MATCH FOUND!\n");
            return true;
        }
        p++;
    }
    HLK_LOG("mgr_parse_response: NO MATCH\n");
    return false;
}

// ===========================
// 发送下一条命令
// ===========================
static void mgr_send_next_cmd(void) {
    HLK_LOG("mgr_send_next_cmd: cmd_index=%d, CMD_COUNT=%d\n", mgr.cmd_index, CMD_COUNT);
    
    if (mgr.cmd_index >= CMD_COUNT) {
        HLK_LOG("mgr_send_next_cmd: All config done, waiting connection\n");
        mgr_update_status(HLK_MGR_WAIT_WIFI);
        mgr.wifi_retry = 0;
        mgr.socket_retry = 0;
        mgr.last_check_tick = HLK_GET_TICK();
        return;
    }
    
    const at_cmd_t *cmd = &wifi_cmds[mgr.cmd_index];
    HLK_LOG("mgr_send_next_cmd: CMD[%d/%d]: %s, expect='%s', timeout=%dms\n", 
            mgr.cmd_index + 1, CMD_COUNT, cmd->cmd, cmd->expect ? cmd->expect : "NULL", cmd->timeout_ms);
    
    mgr.waiting_response = true;
    mgr.cmd_start_tick = HLK_GET_TICK();
    memset(mgr.response, 0, sizeof(mgr.response));
    mgr.response_len = 0;
    
    char tx_buf[128];
    snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd->cmd);
    HLK_LOG("mgr_send_next_cmd: sending '%s'\n", tx_buf);
    HLK_Driver_Send((uint8_t*)tx_buf, strlen(tx_buf));
}

// ===========================
// 处理响应
// ===========================
static void mgr_handle_response(void) {
    // HLK_LOG("mgr_handle_response: cmd_index=%d, waiting_response=%d\n", mgr.cmd_index, mgr.waiting_response);
    
    if (mgr.cmd_index >= CMD_COUNT) {
        HLK_LOG("mgr_handle_response: cmd_index >= CMD_COUNT, return\n");
        return;
    }
    
    const at_cmd_t *cmd = &wifi_cmds[mgr.cmd_index];
    
    // 检查超时
    if (mgr.waiting_response) {
        uint32_t elapsed = HLK_GET_TICK() - mgr.cmd_start_tick;
        if (elapsed % 1000 == 0)
            HLK_LOG("mgr_handle_response: waiting, elapsed=%dms, timeout=%dms\n", elapsed, cmd->timeout_ms);
        
        if (elapsed > cmd->timeout_ms) {
            HLK_LOG("mgr_handle_response: TIMEOUT: %s\n", cmd->cmd);
            mgr.cmd_retry++;
            if (mgr.cmd_retry >= cmd->max_retry) {
                HLK_LOG("mgr_handle_response: Max retry, reconfig\n");
                mgr_start_reconfig();
            } else {
                HLK_LOG("mgr_handle_response: retry %d/%d\n", mgr.cmd_retry, cmd->max_retry);
                mgr_send_next_cmd();
            }
        }
        return;
    }
    
    // 检查响应
    HLK_LOG("mgr_handle_response: checking response: '%s'\n", mgr.response);
    if (mgr_parse_response(mgr.response, cmd->expect)) {
        HLK_LOG("mgr_handle_response: OK! cmd_index %d -> %d\n", mgr.cmd_index, mgr.cmd_index + 1);
        mgr.cmd_index++;
        mgr.cmd_retry = 0;
        mgr_send_next_cmd();
    } else {
        HLK_LOG("mgr_handle_response: UNEXPECTED: %s\n", mgr.response);
        mgr.cmd_retry++;
        if (mgr.cmd_retry >= cmd->max_retry) {
            HLK_LOG("mgr_handle_response: Max retry, reconfig\n");
            mgr_start_reconfig();
        } else {
            HLK_LOG("mgr_handle_response: retry %d/%d\n", mgr.cmd_retry, cmd->max_retry);
            mgr_send_next_cmd();
        }
    }
}

// ===========================
// 等待模块稳定
// ===========================
static void mgr_wait_stable(void) {
    if (!mgr.is_waiting_stable) {
        HLK_LOG("mgr_wait_stable: start waiting, first_connect=%d\n", mgr.is_first_connect);
        mgr.is_waiting_stable = true;
        mgr.wait_start_tick = HLK_GET_TICK();
        return;
    }
    
    uint32_t elapsed = HLK_GET_TICK() - mgr.wait_start_tick;
    uint32_t wait_time = mgr.is_first_connect ? HLK_POWER_ON_WAIT : HLK_RECONFIG_WAIT;
    
    if (elapsed % 1000 == 0)
        HLK_LOG("mgr_wait_stable: elapsed=%d/%dms\n", elapsed, wait_time);
    
    if (elapsed >= wait_time) {
        HLK_LOG("mgr_wait_stable: wait complete, starting config\n");
        mgr.is_waiting_stable = false;
        
        HLK_LOG("mgr_wait_stable: GPIO_ActiveReset\n");
        HLK_GPIO_ActiveReset();
        HLK_LOG("mgr_wait_stable: GPIO_ActiveATMode\n");
        HLK_GPIO_ActiveATMode();
        
        mgr_update_status(HLK_MGR_CONFIGURING);
        mgr.cmd_index = 0;
        mgr.cmd_retry = 0;
        mgr_send_next_cmd();
    }
}

// ===========================
// 检查连接状态
// ===========================
static void mgr_check_connection(void) {
    uint32_t now = HLK_GET_TICK();
    if (now - mgr.last_check_tick < 1000) return;
    mgr.last_check_tick = now;
    
    HLK_LOG("mgr_check_connection: status=%s, WIFI_STS=%d, WIFI_SOT=%d\n", 
            mgr_status_str(mgr.status), WIFI_STS_READ(), WIFI_SOT_READ());
    
    switch (mgr.status) {
        case HLK_MGR_WAIT_WIFI:
            if (WIFI_STS_READ()) {
                HLK_LOG("mgr_check_connection: WiFi connected\n");
                mgr_update_status(HLK_MGR_WAIT_SOCKET);
                mgr.socket_retry = 0;
            } else {
                mgr.wifi_retry++;
                HLK_LOG("mgr_check_connection: waiting WiFi %d/%d\n", mgr.wifi_retry, HLK_LINK_RERTY);
                if (mgr.wifi_retry >= HLK_LINK_RERTY) {
                    HLK_LOG("mgr_check_connection: WiFi timeout\n");
                    mgr_start_reconfig();
                }
            }
            break;
            
        case HLK_MGR_WAIT_SOCKET:
            if (WIFI_SOT_READ()) {
                HLK_LOG("mgr_check_connection: Socket connected, READY!\n");
                mgr_update_status(HLK_MGR_READY);
                mgr.reconnect_fail_count = 0;
            } else {
                mgr.socket_retry++;
                HLK_LOG("mgr_check_connection: waiting Socket %d/%d\n", mgr.socket_retry, HLK_LINK_RERTY);
                if (mgr.socket_retry >= HLK_LINK_RERTY) {
                    HLK_LOG("mgr_check_connection: Socket timeout\n");
                    mgr_start_reconfig();
                }
            }
            break;
            
        case HLK_MGR_READY:
            if (!WIFI_STS_READ() || !WIFI_SOT_READ()) {
                HLK_LOG("mgr_check_connection: Connection lost!\n");
                mgr_start_reconfig();
            }
            break;
            
        default:
            break;
    }
}

// ===========================
// 开始重连
// ===========================
static void mgr_start_reconfig(void) {
    HLK_LOG("mgr_start_reconfig: START\n");
    mgr.is_first_connect = false;
    mgr.cmd_index = 0;
    mgr.cmd_retry = 0;
    mgr.waiting_response = false;
    mgr.wifi_retry = 0;
    mgr.socket_retry = 0;
    mgr.uart_error_count = 0;
    mgr.is_waiting_stable = false;
    mgr.reconnect_fail_count++;
    
    HLK_LOG("mgr_start_reconfig: reconnect_fail_count=%d\n", mgr.reconnect_fail_count);
    
    if (mgr.reconnect_fail_count >= 3) {
        HLK_LOG("mgr_start_reconfig: Reconnect fail 3 times, offline\n");
        mgr_update_status(HLK_MGR_OFFLINE);
        return;
    }
    
    mgr_update_status(HLK_MGR_RECONFIG);
    mgr_wait_stable();
}

// ===========================
// 主任务
// ===========================
static void mgr_task(void *arg) {
    (void)arg;
    
    HLK_LOG("mgr_task: START\n");
    
    HLK_LOG("mgr_task: Driver_Init\n");
    HLK_Driver_Init();

    // 注册驱动回调
    HLK_LOG("mgr_task: RegisterCallbacks\n");
    HLK_Driver_RegisterCallbacks(on_rx, on_tx, on_err);
        
    // GPIO初始化
    HLK_LOG("mgr_task: GPIO_Init\n");
    HLK_GPIO_Init();
    
    // 开始上电连接
    HLK_LOG("mgr_task: start power on\n");
    mgr.is_first_connect = true;
    mgr.reconnect_fail_count = 0;
    mgr_update_status(HLK_MGR_IDLE);
    mgr_wait_stable();
    
    HLK_LOG("mgr_task: entering main loop\n");
    while (1) {
        switch (mgr.status) {
            case HLK_MGR_IDLE:
            case HLK_MGR_RECONFIG:
                mgr_wait_stable();
                break;
                
            case HLK_MGR_CONFIGURING:
                mgr_handle_response();
                break;
                
            case HLK_MGR_WAIT_WIFI:
            case HLK_MGR_WAIT_SOCKET:
            case HLK_MGR_READY:
                mgr_check_connection();
                break;
                
            case HLK_MGR_OFFLINE: {
                static uint32_t offline_start = 0;
                if (offline_start == 0) {
                    offline_start = HLK_GET_TICK();
                    HLK_LOG("mgr_task: OFFLINE start at %d\n", offline_start);
                }
                if (HLK_GET_TICK() - offline_start > 30000) {
                    HLK_LOG("mgr_task: OFFLINE 30s elapsed, retry\n");
                    offline_start = 0;
                    mgr.reconnect_fail_count = 0;
                    mgr_start_reconfig();
                }
                break;
            }
                
            case HLK_MGR_ERROR:
            default:
                HLK_LOG("mgr_task: ERROR state, wait 3s then reconfig\n");
                HLK_Delay_ms(3000);
                mgr_start_reconfig();
                break;
        }
        HLK_Delay_ms(50);
    }
}

// ===========================
// 对外接口
// ===========================
void HLK_Manager_Init(void) {
    if (mgr.initialized) {
        HLK_LOG("HLK_Manager_Init: already initialized\n");
        return;
    }
    
    HLK_LOG("HLK_Manager_Init: creating task\n");
    
    #if HLK_USE_RTOS
    xTaskCreate(mgr_task, "WiFiMgr", 4096, NULL, 3, &mgr.task_handle);
    #endif
    
    mgr.initialized = true;
    HLK_LOG("HLK_Manager_Init: done\n");
}