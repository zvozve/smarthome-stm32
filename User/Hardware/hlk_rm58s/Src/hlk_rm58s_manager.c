#include "hlk_rm58s_manager.h"
#include "hlk_rm58s_port.h"
#include "hlk_rm58s_config.h"
#include "hlk_rm58s_at.h"
#include <string.h>
#include <ctype.h>

// ===========================
// 私有数据
// ===========================
static struct {
    mgr_status_t status;
    bool response_ready;
    char response_buf[HLK_UART_BUF_LEN];
    uint16_t response_len;
    
    // 配置相关
    const char **config_cmds;
    uint8_t config_index;
    uint8_t config_retry;
    
    // 等待相关
    uint8_t wait_retry;
    uint32_t wait_tick;
    uint8_t wifi_retry;
    uint8_t socket_retry;
    uint8_t mac_queried;
    
    // 回调
    mgr_rx_callback_t rx_callback;
} mgr_ctx = {
    .status = MGR_IDLE,
    .response_ready = false,
    .config_index = 0,
    .wait_retry = 0,
    .config_retry = 0,
    .mac_queried = 0,
    .rx_callback = NULL,
};

// ===========================
// 配置指令序列
// ===========================
static const char *wifi_config_cmds[] = {
    AT_NETMODE_STA,
    AT_NETMODE_STA,
    AT_WIFI_CONF,
    AT_DHCPC_ON,
    AT_REMOTEIP,
    AT_REMOTEPORT,
    AT_QUERY_MAC,
    AT_REMOTEPRO,
    AT_MODE_CLIENT,
    AT_UART,
    AT_UARTPACKLEN,
    AT_UARTPACKTIMEOUT,
    AT_NET_COMMIT,
    // AT_RECONN,
    NULL
};

// ===========================
// 内部函数声明
// ===========================
static int mgr_parse_response(const char *response, const char *expected);
static void mgr_send_next_config(void);
static void mgr_start_reconfig(void);
static void mgr_handle_config_response(void);
static void mgr_check_wifi(void);
static void mgr_check_socket(void);

// ===========================
// 解析响应
// ===========================
static int mgr_parse_response(const char *response, const char *expected) {
    if (!response) return 1;
    if (expected && strstr(response, expected)) return 0;
    if (strcasestr_local(response, "ok")) return 0;
    if (strcasestr_local(response, "error") || strcasestr_local(response, "fail")) return 1;
    return 1;
}

// ===========================
// 驱动层回调
// ===========================
void HLK_Manager_OnResponseComplete(void) {
    // mgr_ctx.response_ready = true;
    mgr_ctx.response_len = hlk_task.rxAccumLen;
    memcpy(mgr_ctx.response_buf, hlk_task.rxAccumBuf, mgr_ctx.response_len);
    mgr_ctx.response_buf[mgr_ctx.response_len] = '\0';
    HLK_LOG("Manager: Response complete!\n");
    
    // 透传模式：数据转发给用户回调，不触发状态机
    if (mgr_ctx.status == MGR_READY) {
        if (mgr_ctx.rx_callback) {
            mgr_ctx.rx_callback(mgr_ctx.response_buf, mgr_ctx.response_len);
        }
        // 透传数据不触发 response_ready
        mgr_ctx.response_ready = false;
        return;
    }
    
    // AT 模式：触发状态机处理
    mgr_ctx.response_ready = true;
}

void HLK_Manager_OnTimeout(void) {
    mgr_ctx.response_ready = true;
    mgr_ctx.response_len = 0;
    HLK_LOG("Manager: Timeout!\n");
}

void HLK_Manager_OnError(void) {
    mgr_ctx.response_ready = true;
    mgr_ctx.response_len = 0;
    HLK_LOG("Manager: Error!\n");
}

// ===========================
// 内部函数：发送配置指令
// ===========================
static void mgr_send_next_config(void) {
    const char *cmd = mgr_ctx.config_cmds[mgr_ctx.config_index];

    if (cmd != NULL) {
        HLK_Driver_Transmit(cmd);
    } else {
        // 所有配置完成，发 reconn 后等待连接
        HAL_UART_Transmit_DMA(&huart_hlk, (uint8_t*)AT_RECONN "\r\n", strlen(AT_RECONN) + 2);
        HLK_Delay_ms(2000);
        mgr_ctx.status = MGR_WAIT_WIFI;
        mgr_ctx.wifi_retry = 0;
        mgr_ctx.socket_retry = 0;
        mgr_ctx.wait_tick = 0;
    }
}

// ===========================
// 内部函数：重新配置
// ===========================
static void mgr_start_reconfig(void) {
    HLK_LOG("Start WiFi config...\n");
    HLK_Driver_SetATMode(true);
    mgr_ctx.config_cmds = wifi_config_cmds;
    mgr_ctx.config_index = 0;
    mgr_ctx.config_retry = 0;
    mgr_ctx.wait_retry = 0;
    mgr_ctx.wifi_retry = 0;
    mgr_ctx.socket_retry = 0;
    HLK_GPIO_ActiveReset();
    HLK_Delay_ms(3000);
    HLK_GPIO_ActiveATMode();
    mgr_ctx.status = MGR_CONFIGURING;
    HLK_Driver_Transmit(wifi_config_cmds[0]);
}

// ===========================
// 处理配置响应
// ===========================
static void mgr_handle_config_response(void) {
    if (mgr_parse_response(mgr_ctx.response_buf, "ok") == 0) {
        HLK_LOG("Config OK: %s\n", mgr_ctx.config_cmds[mgr_ctx.config_index]);
        mgr_ctx.config_index++;
        mgr_ctx.config_retry = 0;
        mgr_send_next_config();
    } else {
        mgr_ctx.config_retry++;
        if (mgr_ctx.config_retry >= 3) {
            HLK_LOG("Config max retry, reconfig...\n");
            mgr_ctx.config_retry = 0;
            mgr_start_reconfig();
        } else {
            HLK_LOG("Config failed, retry %d\n", mgr_ctx.config_retry);
            HLK_Driver_Transmit(mgr_ctx.config_cmds[mgr_ctx.config_index]);
        }
    }
}

// ===========================
// 检查WiFi状态
// ===========================
static void mgr_check_wifi(void) {
    if (WIFI_STS_READ()) {
        HLK_LOG("WiFi connected.\n");
        mgr_ctx.status = MGR_WAIT_SOCKET;
        mgr_ctx.socket_retry = 0;
        return;
    }

    mgr_ctx.wifi_retry++;
    HLK_LOG("Waiting WiFi... %d s\n", mgr_ctx.wifi_retry);
    if (mgr_ctx.wifi_retry >= HLK_LINK_RERTY) {
        HLK_LOG("WiFi timeout, reconfig...\n");
        mgr_ctx.wifi_retry = 0;
        mgr_start_reconfig();
    }
}

// ===========================
// 检查Socket状态
// ===========================
static void mgr_check_socket(void) {
    if (WIFI_SOT_READ()) {
        HLK_LOG("Socket connected.\n");
        mgr_ctx.status = MGR_READY;
        mgr_ctx.socket_retry = 0;
        HLK_LOG("WiFi module ready!\n");
        return;
    }
    
    mgr_ctx.socket_retry++;
    HLK_LOG("Waiting Socket... %d s \n", mgr_ctx.socket_retry);
    if (mgr_ctx.socket_retry >= HLK_LINK_RERTY) {
        HLK_LOG("Socket timeout, reconfig...\n");
        mgr_ctx.socket_retry = 0;
        mgr_start_reconfig();
    }
}

// ===========================
// Manager 初始化
// ===========================
void HLK_Manager_Init(void) {
    HLK_Driver_RegisterCallbacks(
        HLK_Manager_OnResponseComplete,
        HLK_Manager_OnTimeout,
        HLK_Manager_OnError
    );
    HLK_Driver_Init();
    
    HAL_TIM_Base_Start_IT(&htim_hlk);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart_hlk, hlk_task.rxBuf, HLK_UART_BUF_LEN);
    
    // 等待模块启动
    HLK_Delay_ms(5000);    
    HLK_GPIO_Init();    
    // 开始配置
    mgr_start_reconfig();
}

// ===========================
// Manager 主状态机
// ===========================
void HLK_Manager_Task(void) {
    // HLK_LOG("mgr_ctx.status: %d\n", mgr_ctx.status);

    switch (mgr_ctx.status) {
        case MGR_IDLE:
            break;

        case MGR_CONFIGURING:
            if (!mgr_ctx.response_ready) break;
            mgr_ctx.response_ready = false;
            
            // MAC查询处理
            if (mgr_ctx.mac_queried == 0 && mgr_ctx.response_len > 0) {
                char *p = strstr(mgr_ctx.response_buf, "Get_MAC");
                if (p || strstr(mgr_ctx.response_buf, ":")) {
                    mgr_ctx.mac_queried = 1;
                    char *start = strstr(mgr_ctx.response_buf, "Get_MAC=");
                    if (start) {
                        start += 9;
                        char *end = strstr(start, "\r\n");
                        if (end) {
                            char mac[32] = {0};
                            int len = end - start;
                            memcpy(mac, start, len > 31 ? 31 : len);
                            HLK_LOG("MAC: %s\n", mac);
                        }
                    }
                    break;
                }
            }
            
            mgr_handle_config_response();
            break;
            
        case MGR_WAIT_WIFI:
            mgr_check_wifi();
            break;

        case MGR_WAIT_SOCKET:
            mgr_check_socket();
            break;

        case MGR_READY:
            if (HLK_Driver_GetATMode()) {
                HLK_Driver_SetATMode(false);
            }
            if (!WIFI_STS_READ()) {
                HLK_LOG("WiFi disconnected! Reconfig...\n");
                mgr_start_reconfig();
                break;
            }
            if (!WIFI_SOT_READ()) {
                HLK_LOG("Socket disconnected! Reconfig...\n");
                mgr_start_reconfig();
                break;
            }
            // 透传模式下收到的数据，忽略 response_ready
            if (mgr_ctx.response_ready) {
                mgr_ctx.response_ready = false;  // 清空，不处理
            }
            break;

        case MGR_RECONFIG:
            mgr_start_reconfig();
            break;
    }
}

// ===========================
// Manager API 实现
// ===========================
void HLK_Manager_Reconfig(void) {
    mgr_start_reconfig();
}

mgr_status_t HLK_Manager_GetStatus(void) {
    return mgr_ctx.status;
}

bool HLK_Manager_IsReady(void) {
    return (mgr_ctx.status == MGR_READY);
}

void HLK_Manager_RegisterRxCallback(mgr_rx_callback_t cb) {
    mgr_ctx.rx_callback = cb;
}