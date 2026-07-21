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
#define HLK_MAX_RX_CALLBACKS    4           // 最大回调数（新增）
#define HLK_RECONFIG_WAIT       5000
#define HLK_RECONNECT_RETRY     3
#define HLK_LINK_CHECK_INTERVAL 1000
#define HLK_LINK_RERTY          20

// ===========================
// AT指令项
// ===========================
typedef struct {
    const char *cmd;
    const char *expect;
    uint32_t timeout_ms;
    uint8_t max_retry;
} at_cmd_t;

// ===========================
// AT指令表
// ===========================
static const at_cmd_t wifi_cmds[] = {
    {AT_NETMODE_STA,            "ok", 3000, 3},
    {AT_NETMODE_STA,            "ok", 3000, 3},
    {AT_WIFI_CONF,              "ok", 5000, 3},
    {AT_DHCPC_ON,               "ok", 3000, 3},
    {AT_REMOTEIP,               "ok", 3000, 3},
    {AT_REMOTEPORT,             "ok", 3000, 3},
    {AT_QUERY_MAC,              "Get_MAC", 3000, 2},
    {AT_REMOTEPRO,              "ok", 2000, 2},
    {AT_MODE_CLIENT,            "ok", 2000, 2},
    {AT_UART,                   "ok", 3000, 3},
    {AT_UARTPACKLEN,            "ok", 3000, 3},
    {AT_UARTPACKTIMEOUT,        "ok", 3000, 3},
    {AT_NET_COMMIT,             "ok", 5000, 5},
    {AT_RECONN,                 "ok", 2000, 2},
};

#define CMD_COUNT (sizeof(wifi_cmds) / sizeof(at_cmd_t))

// ===========================
// 上下文
// ===========================
static struct {
    hlk_mgr_status_t status;
    
    // 配置状态机
    struct {
        uint8_t cmd_index;
        uint8_t cmd_retry;
        uint32_t cmd_start_tick;
        bool waiting_response;
        char response[512];
        uint16_t response_len;
        uint32_t last_rx_tick;
    } config;
    
    // 等待稳定
    struct {
        bool active;
        uint32_t start_tick;
    } wait;
    
    // 连接检测
    struct {
        uint32_t check_tick;
        uint8_t retry_count;
    } conn;
    
    // 重连管理
    struct {
        uint8_t fail_count;
        uint32_t offline_start_tick;
    } reconnect;
    
    // 回调列表（修改：单个回调 → 回调列表）
    struct {
        mgr_rx_callback_t rx_callbacks[HLK_MAX_RX_CALLBACKS];
        uint8_t rx_count;
        mgr_status_callback_t status_callback;
    } cb;
    
    TaskHandle_t task_handle;
    bool initialized;
} mgr = {0};

// ===========================
// 函数声明
// ===========================
static void mgr_task(void *arg);
static void mgr_state_configuring(void);
static void mgr_state_disconnected(void);
static void mgr_state_connected(void);

static void mgr_start_config(void);
static void mgr_start_wait(void);
static void mgr_send_next_cmd(void);
static void mgr_handle_response(void);
static void mgr_update_status(hlk_mgr_status_t status);
static bool mgr_parse_response(const char *resp, const char *expect);
static const char* mgr_status_str(hlk_mgr_status_t s);

// ===========================
// 驱动回调
// ===========================
static void on_rx(uint8_t *data, uint16_t len) {
    if (len == 0 || data == NULL) return;
    
    // 配置模式下处理AT响应
    if (mgr.status == HLK_MGR_CONFIGURING) {
        if (mgr.config.waiting_response) {
            if (mgr.config.response_len + len < sizeof(mgr.config.response) - 1) {
                memcpy(mgr.config.response + mgr.config.response_len, data, len);
                mgr.config.response_len += len;
                mgr.config.response[mgr.config.response_len] = '\0';
                // HLK_LOG("RX %s\n", mgr.config.response);
                mgr.config.last_rx_tick = HLK_GET_TICK();
            }
        }
        return;
    }
    
    // 已连接模式下：分发给所有注册的任务
    if (mgr.status == HLK_MGR_CONNECTED) {
        for (int i = 0; i < mgr.cb.rx_count; i++) {
            if (mgr.cb.rx_callbacks[i]) {
                mgr.cb.rx_callbacks[i](data, len);
            }
        }
        return;
    }
}

static void on_tx(void) {
    // 空
}

static void on_err(void) {
    if (mgr.status != HLK_MGR_CONFIGURING) {
        mgr_start_config();
    }
}

// ===========================
// 状态工具
// ===========================
static const char* mgr_status_str(hlk_mgr_status_t s) {
    switch(s) {
        case HLK_MGR_DISCONNECTED: return "DISCONNECTED";
        case HLK_MGR_CONNECTED:    return "CONNECTED";
        case HLK_MGR_CONFIGURING:  return "CONFIGURING";
        default:                   return "UNKNOWN";
    }
}

static void mgr_update_status(hlk_mgr_status_t status) {
    if (mgr.status != status) {
        HLK_LOG("mgr_update_status: %s -> %s\n", 
                mgr_status_str(mgr.status), mgr_status_str(status));
        mgr.status = status;
        if (mgr.cb.status_callback) {
            mgr.cb.status_callback(status);
        }
    }
}

// ===========================
// 响应解析
// ===========================
static bool mgr_parse_response(const char *resp, const char *expect) {
    if (!resp || strlen(resp) == 0) return false;
    if (expect == NULL) return true;
    
    const char *p = resp;
    while (*p) {
        const char *p2 = p;
        const char *e2 = expect;
        while (*p2 && *e2 && tolower(*p2) == tolower(*e2)) {
            p2++; e2++;
        }
        if (!*e2) return true;
        p++;
    }
    return false;
}

// ===========================
// 配置状态 - 发送下一条命令
// ===========================
static void mgr_send_next_cmd(void) {
    if (mgr.config.cmd_index >= CMD_COUNT) {
        // 所有AT命令发送完成
        
        // // 1. 发送重启命令退出AT模式
        // HLK_LOG("All config done, send reconn\n");
        // char tx_buf[32];
        // snprintf(tx_buf, sizeof(tx_buf), "at+reconn=1\r\n");
        // HLK_Driver_Send((uint8_t*)tx_buf, strlen(tx_buf));
        
        // 硬件复位
        HLK_GPIO_ActiveReset();
                
        // 4. 进入未连接状态
        HLK_LOG("Config complete, enter DISCONNECTED\n");
        mgr_update_status(HLK_MGR_DISCONNECTED);
        mgr.conn.check_tick = HLK_GET_TICK();
        mgr.conn.retry_count = 0;
        return;
    }
    
    const at_cmd_t *cmd = &wifi_cmds[mgr.config.cmd_index];
    HLK_LOG("CMD[%d/%d]: %s\n", mgr.config.cmd_index + 1, CMD_COUNT, cmd->cmd);
    
    mgr.config.waiting_response = true;
    mgr.config.cmd_start_tick = HLK_GET_TICK();
    mgr.config.last_rx_tick = mgr.config.cmd_start_tick;
    mgr.config.response_len = 0;
    memset(mgr.config.response, 0, sizeof(mgr.config.response));
    
    char tx_buf[64];
    snprintf(tx_buf, sizeof(tx_buf), "%s\r\n", cmd->cmd);
    HLK_Driver_Send((uint8_t*)tx_buf, strlen(tx_buf));
}

// ===========================
// 配置状态 - 处理响应
// ===========================
static void mgr_handle_response(void) {
    if (mgr.config.cmd_index >= CMD_COUNT) return;
    
    const at_cmd_t *cmd = &wifi_cmds[mgr.config.cmd_index];
    uint32_t elapsed = HLK_GET_TICK() - mgr.config.cmd_start_tick;
    
    // 超时处理
    if (elapsed > cmd->timeout_ms) {
        HLK_LOG("TIMEOUT: %s\n", cmd->cmd);
        mgr.config.waiting_response = false;
        mgr.config.cmd_retry++;
        
        if (mgr.config.cmd_retry >= cmd->max_retry) {
            HLK_LOG("Max retry, reconfig\n");
            mgr_start_config();
        } else {
            mgr_send_next_cmd();
        }
        return;
    }
    
    // 等待响应
    if (mgr.config.waiting_response) {
        if (mgr.config.response_len > 0) {
            // 检查响应是否匹配
            bool matched = mgr_parse_response(mgr.config.response, cmd->expect);
            bool has_newline = (mgr.config.response[mgr.config.response_len - 1] == '\n' ||
                               mgr.config.response[mgr.config.response_len - 1] == '\r');
            
            if (matched) {
                HLK_LOG("CMD OK: %s\n", cmd->cmd);
                mgr.config.cmd_index++;
                mgr.config.cmd_retry = 0;
                mgr.config.waiting_response = false;
                mgr_send_next_cmd();
            } else if (has_newline) {
                // 收到完整响应但不匹配
                HLK_LOG("Unexpected response\n");
                mgr.config.waiting_response = false;
                mgr.config.cmd_retry++;
                if (mgr.config.cmd_retry >= cmd->max_retry) {
                    mgr_start_config();
                } else {
                    mgr_send_next_cmd();
                }
            }
        }
    }
}

// ===========================
// 配置状态主函数
// ===========================
static void mgr_state_configuring(void) {
    // 等待模块稳定
    if (mgr.wait.active) {
        uint32_t elapsed = HLK_GET_TICK() - mgr.wait.start_tick;
        uint32_t wait_time = HLK_RECONFIG_WAIT;
        
        if (elapsed >= wait_time) {
            HLK_LOG("Wait complete, start AT config\n");
            mgr.wait.active = false;
            
            // 3. 进入AT模式（拉低ES）
            HLK_GPIO_EnterATMode();

            // 5. 开始发送AT命令
            mgr.config.cmd_index = 0;
            mgr.config.cmd_retry = 0;
            mgr_send_next_cmd();
        }
        return;
    }
    
    // 处理AT命令
    mgr_handle_response();
}

// ===========================
// 未连接状态主函数
// ===========================
static void mgr_state_disconnected(void) {
    uint32_t now = HLK_GET_TICK();
    
    // 检测连接
    if (now - mgr.conn.check_tick < HLK_LINK_CHECK_INTERVAL) return;
    mgr.conn.check_tick = now;
    
    bool sta = WIFI_STS_READ();
    bool sot = WIFI_SOT_READ();
    
    HLK_LOG("Check: STA=%d SOT=%d retry=%d/%d\n", 
            sta, sot, mgr.conn.retry_count, HLK_LINK_RERTY);
    
    // STA和SOT都高 => 已连接
    if (sta && sot) {
        HLK_LOG("Connected!\n");
        mgr_update_status(HLK_MGR_CONNECTED);
        mgr.reconnect.fail_count = 0;
        mgr.conn.retry_count = 0;
        return;
    }
    
    // 超时 -> 重新配置
    mgr.conn.retry_count++;
    if (mgr.conn.retry_count >= HLK_LINK_RERTY) {
        HLK_LOG("Connection timeout, reconfig\n");
        mgr.conn.retry_count = 0;
        mgr_start_config();
    }
}

// ===========================
// 已连接状态主函数
// ===========================
static void mgr_state_connected(void) {
    uint32_t now = HLK_GET_TICK();
    
    // 检测连接
    if (now - mgr.conn.check_tick < HLK_LINK_CHECK_INTERVAL) return;
    mgr.conn.check_tick = now;
    
    bool sta = WIFI_STS_READ();
    bool sot = WIFI_SOT_READ();
    
    // STA或SOT低 => 连接丢失
    if (!sta || !sot) {
        HLK_LOG("Connection lost! STA=%d SOT=%d\n", sta, sot);
        mgr_start_config();
    }
}

// ===========================
// 开始配置
// ===========================
static void mgr_start_config(void) {
    HLK_LOG("Start config\n");
    
    // 重置配置状态
    mgr.config.cmd_index = 0;
    mgr.config.cmd_retry = 0;
    mgr.config.waiting_response = false;
    mgr.config.response_len = 0;
    mgr.conn.retry_count = 0;
    
    // 增加失败计数
    mgr.reconnect.fail_count++;
    HLK_LOG("Reconnect fail %d / %d times, offline\n", mgr.reconnect.fail_count, HLK_RECONNECT_RETRY);
    if (mgr.reconnect.fail_count >= HLK_RECONNECT_RETRY) {
        mgr.reconnect.fail_count = 0;
        mgr_update_status(HLK_MGR_DISCONNECTED);
        mgr.reconnect.offline_start_tick = HLK_GET_TICK();
        // 进入配置状态，等待稳定
        mgr_update_status(HLK_MGR_CONFIGURING);
        mgr_start_wait();
        return;
    }    
}

// ===========================
// 开始等待稳定
// ===========================
static void mgr_start_wait(void) {
    mgr.wait.active = true;
    mgr.wait.start_tick = HLK_GET_TICK();
    HLK_LOG("Wait stable...\n");
}

// ===========================
// 主任务
// ===========================
static void mgr_task(void *arg) {
    (void)arg;
    
    HLK_LOG("mgr_task START\n");
    
    HLK_Driver_Init();
    HLK_Driver_RegisterCallbacks(on_rx, on_tx, on_err);
    HLK_GPIO_Init();
    
    // 首次上电
    mgr_update_status(HLK_MGR_CONFIGURING);
    mgr_start_wait();
    mgr.reconnect.fail_count = 0;
    
    HLK_LOG("Enter main loop\n");
    
#if 0
    HLK_GPIO_ActiveRecoveryMode();
    while (1) {
        HLK_LOG("WHILE(1)\n");
        HLK_Delay_ms(1000);
    }
#else 
    while (1) {
        switch (mgr.status) {
            case HLK_MGR_CONFIGURING:
                mgr_state_configuring();
                break;
                
            case HLK_MGR_DISCONNECTED:
                mgr_state_disconnected();
                break;
                
            case HLK_MGR_CONNECTED:
                mgr_state_connected();
                break;
                
            default:
                break;
        }
        HLK_Delay_ms(50);
    }
#endif     
}

// ===========================
// 回调注册接口
// ===========================
int HLK_Manager_RegisterRxCallback(mgr_rx_callback_t callback) {
    if (!callback) return -1;
    if (mgr.cb.rx_count >= HLK_MAX_RX_CALLBACKS) return -2;
    
    for (int i = 0; i < mgr.cb.rx_count; i++) {
        if (mgr.cb.rx_callbacks[i] == callback) {
            return -3;  // 已注册
        }
    }
    
    mgr.cb.rx_callbacks[mgr.cb.rx_count++] = callback;
    HLK_LOG("Manager: RegisterRx count=%d\n", mgr.cb.rx_count);
    return 0;
}

int HLK_Manager_UnregisterRxCallback(mgr_rx_callback_t callback) {
    if (!callback) return -1;
    
    for (int i = 0; i < mgr.cb.rx_count; i++) {
        if (mgr.cb.rx_callbacks[i] == callback) {
            for (int j = i; j < mgr.cb.rx_count - 1; j++) {
                mgr.cb.rx_callbacks[j] = mgr.cb.rx_callbacks[j + 1];
            }
            mgr.cb.rx_count--;
            mgr.cb.rx_callbacks[mgr.cb.rx_count] = NULL;
            return 0;
        }
    }
    return -2;
}

void HLK_Manager_RegisterStatusCallback(mgr_status_callback_t callback) {
    mgr.cb.status_callback = callback;
    HLK_LOG("Manager: RegisterStatus callback\n");
}

// ===========================
// 对外接口
// ===========================
void HLK_Manager_Init(void) {
    if (mgr.initialized) return;
    
#if HLK_USE_RTOS
    xTaskCreate(mgr_task, "WiFiMgr", 4096, NULL, 3, &mgr.task_handle);
#endif
    
    mgr.initialized = true;
}

hlk_mgr_status_t HLK_Manager_GetStatus(void) {
    return mgr.status;
}