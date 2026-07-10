#include "hlk_rm58s.h"
#include "hlk_rm58s_config.h"
#include "hlk_rm58s_rtos.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h> 
#include "stm32f4xx_hal.h"

// ===========================
// 外部HAL函数（由hlk_rm58s_hal.c实现）
// ===========================

extern int  hal_uart_send(void *handle, uint8_t *data, uint16_t len);
extern int  hal_uart_recv_start(void *handle, uint8_t *buf, uint16_t len);
extern void hal_uart_abort(void *handle);

// ===========================
// 内部数据结构
// ===========================

typedef struct {
    void *uart_handle;

    hlk_state_t state;
    bool wifi_connected;
    bool tcp_connected;

    hlk_rx_callback_t rx_callback;
    hlk_state_callback_t state_callback;

    uint32_t at_timeout_ms;
    uint32_t connect_timeout_ms;

    uint8_t rx_buf[HLK_RX_BUF_SIZE];
    uint16_t rx_len;
    bool rx_complete;

    char at_response[HLK_AT_RESP_BUF_SIZE];
    uint16_t at_resp_len;
    bool at_waiting;
    uint32_t at_start_tick;
} hlk_priv_t;

static hlk_priv_t g_hlk = {0};

// ===========================
// 内部函数
// ===========================

static char *strcasestr_local(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
            h++; n++;
        }
        if (!*n) return (char *)haystack;
    }
    return NULL;
}

static bool str_contains(const char *str, const char *sub) {
    return strcasestr_local(str, sub) != NULL;
}

static void hlk_set_state(hlk_state_t state) {
    g_hlk.state = state;
}

static void hlk_update_state(void) {
    bool wifi = hal_gpio_read_sts() != 0;
    bool tcp = hal_gpio_read_sot() != 0;

    if (wifi != g_hlk.wifi_connected || tcp != g_hlk.tcp_connected) {
        g_hlk.wifi_connected = wifi;
        g_hlk.tcp_connected = tcp;
        if (g_hlk.state_callback) {
            g_hlk.state_callback(wifi, tcp);
        }
    }
}

static void hlk_uart_start_recv(void) {
    hal_uart_recv_start(g_hlk.uart_handle, g_hlk.rx_buf, HLK_RX_BUF_SIZE);
}

// ===========================
// 公共API实现
// ===========================

int hlk_rm58s_init(hlk_config_t *config) {
    HLK_LOG("hlk_rm58s_init entry\n");
    
    if (!config || !config->uart_handle) {
        HLK_LOG("config invalid\n");
        return -1;
    }

    // 直接赋值，不依赖 memcpy
    g_hlk.uart_handle = config->uart_handle;
    g_hlk.rx_callback = config->rx_callback;
    g_hlk.state_callback = config->state_callback;
    g_hlk.at_timeout_ms = config->at_timeout_ms;
    g_hlk.connect_timeout_ms = config->connect_timeout_ms;

    // ===== GPIO 初始化 =====
    // ES 和 RST 默认拉高
    hal_gpio_write_rst(1);
    hal_gpio_write_es(1);
    HLK_LOG("GPIO initialized: RST=1, ES=1\n");

    // 验证函数指针
    HLK_LOG("hlk_delay_ms=%p, hlk_get_tick=%p\n", hlk_delay_ms, hlk_get_tick);

    g_hlk.state = HLK_STATE_UNINIT;
    g_hlk.wifi_connected = false;
    g_hlk.tcp_connected = false;
    g_hlk.rx_complete = false;
    g_hlk.at_waiting = false;

    if (g_hlk.at_timeout_ms == 0) g_hlk.at_timeout_ms = HLK_AT_TIMEOUT_DEFAULT;
    if (g_hlk.connect_timeout_ms == 0) g_hlk.connect_timeout_ms = HLK_CONNECT_TIMEOUT_DEFAULT;

    HLK_LOG("Calling hlk_rm58s_reset\n");
    hlk_rm58s_reset(HLK_RESET_HOLD_MS);
    
    HLK_LOG("Calling hlk_rm58s_exit_transparent_mode\n");
    hlk_rm58s_exit_transparent_mode();

    HLK_LOG("hlk_rm58s_init done\n");
    return 0;
}

void hlk_rm58s_deinit(void) {
    hal_uart_abort(g_hlk.uart_handle);
    memset(&g_hlk, 0, sizeof(g_hlk));
}

void hlk_rm58s_reset(uint32_t hold_ms) {
    hal_gpio_write_rst(0);
    hlk_delay_ms(hold_ms);
    hal_gpio_write_rst(1);
    hlk_delay_ms(2000);
    hlk_set_state(HLK_STATE_IDLE);
    HLK_LOG("hlk_rm58s_reset done\n");
}

void hlk_rm58s_factory_reset(uint32_t wait_s) {
    hal_gpio_write_es(0);
    for (uint32_t i = wait_s; i > 0; i--) {
        hlk_delay_ms(1000);
    }
    hal_gpio_write_es(1);
    hlk_delay_ms(1000);
    hlk_set_state(HLK_STATE_IDLE);
}

void hlk_rm58s_enter_at_mode(uint32_t hold_ms) {
    HLK_LOG("enter_at_mode, hold=%dms\n", hold_ms);
    
    // 先发送 +++ 退出透传
    HLK_LOG("Sending +++\n");
    hlk_delay_ms(500);
    hal_uart_send(g_hlk.uart_handle, (uint8_t*)"+++", 3);
    hlk_delay_ms(500);

    // 再拉低 ES
    hal_gpio_write_es(0);
    hlk_delay_ms(hold_ms);
    hal_gpio_write_es(1);
    hlk_delay_ms(200);
    hlk_set_state(HLK_STATE_AT_MODE);
    HLK_LOG("enter_at_mode done\n");
}

void hlk_rm58s_exit_transparent_mode(void) {
    HLK_LOG("exit_transparent_mode\n");
    hlk_set_state(HLK_STATE_TRANSPARENT);
    HLK_LOG("calling hal_uart_recv_start\n");
    hlk_uart_start_recv();
    HLK_LOG("hal_uart_recv_start done\n");
}

hlk_state_t hlk_rm58s_get_state(void) {
    return g_hlk.state;
}

// ===========================
// AT命令
// ===========================

int hlk_rm58s_execute_at(const char *cmd, char *response, uint16_t resp_size, uint32_t timeout_ms) {
    if (!cmd || !g_hlk.uart_handle) return -1;

    HLK_LOG("state before AT check: %d\n", g_hlk.state);
    if (g_hlk.state != HLK_STATE_AT_MODE) {
        HLK_LOG("Not in AT mode, entering...\n");
        hlk_rm58s_enter_at_mode(HLK_AT_ENTER_HOLD_MS);
        HLK_LOG("After enter_at_mode, state=%d\n", g_hlk.state);
    } else {
        HLK_LOG("Already in AT mode\n");
    }

    HLK_LOG("Clearing response buffer\n");
    memset(g_hlk.at_response, 0, sizeof(g_hlk.at_response));
    g_hlk.at_resp_len = 0;
    g_hlk.at_waiting = true;
    g_hlk.at_start_tick = hlk_get_tick();
    HLK_LOG("start_tick=%d\n", g_hlk.at_start_tick);

    char cmd_buf[HLK_CMD_BUF_SIZE];
    int cmd_len = snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", cmd);
    HLK_LOG("cmd_len=%d\n", cmd_len);
    if (cmd_len >= (int)sizeof(cmd_buf)) return -1;

    HLK_LOG("Sending: %s", cmd_buf);
    int send_ret = hal_uart_send(g_hlk.uart_handle, (uint8_t*)cmd_buf, cmd_len);
    HLK_LOG("hal_uart_send returned: %d\n", send_ret);

    uint32_t wait_ms = (timeout_ms > 0) ? timeout_ms : g_hlk.at_timeout_ms;
    HLK_LOG("Waiting for response, timeout=%dms\n", wait_ms);
    
    while (g_hlk.at_waiting) {
        if (hlk_get_tick() - g_hlk.at_start_tick > wait_ms) {
            g_hlk.at_waiting = false;
            HLK_LOG("Response timeout after %dms\n", wait_ms);
            return -2;
        }
        hlk_delay_ms(10);
    }

    HLK_LOG("Response: %s\n", g_hlk.at_response);

    if (str_contains(g_hlk.at_response, "OK") || str_contains(g_hlk.at_response, "ok")) {
        if (response && resp_size > 0) {
            strncpy(response, g_hlk.at_response, resp_size - 1);
            response[resp_size - 1] = '\0';
        }
        return 0;
    }

    if (str_contains(g_hlk.at_response, "ERROR") || str_contains(g_hlk.at_response, "error")) {
        return -1;
    }

    return -1;
}

int hlk_rm58s_send_at(const char *cmd) {
    if (!cmd || !g_hlk.uart_handle) return -1;

    char cmd_buf[HLK_CMD_BUF_SIZE];
    int cmd_len = snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", cmd);
    if (cmd_len >= (int)sizeof(cmd_buf)) return -1;

    hal_uart_send(g_hlk.uart_handle, (uint8_t*)cmd_buf, cmd_len);
    return 0;
}

// ===========================
// WiFi连接
// ===========================

int hlk_rm58s_connect_sta(const char *ssid, const char *password, hlk_security_t security) {
    HLK_LOG("connect_sta: ssid=%s, security=%d\n", ssid, security);
    if (!ssid || !password) return -1;

    const char *sec_str = "wpa2_psk";
    switch (security) {
        case HLK_SECURITY_NONE: sec_str = "none"; break;
        case HLK_SECURITY_WEP: sec_str = "wep"; break;
        case HLK_SECURITY_WPA_PSK: sec_str = "wpa_psk"; break;
        case HLK_SECURITY_WPA2_PSK: sec_str = "wpa2_psk"; break;
        case HLK_SECURITY_WPA_WPA2_PSK: sec_str = "wpa_wpa2_psk"; break;
        default: sec_str = "wpa2_psk"; break;
    }

    char cmd[HLK_CMD_BUF_SIZE];
    
    HLK_LOG("Setting netmode=2\n");
    if (hlk_rm58s_execute_at("at+netmode=2", NULL, 0, 0) != 0) {
        HLK_LOG("netmode=2 failed\n");
        return -1;
    }
    HLK_LOG("netmode=2 OK\n");
    
    snprintf(cmd, sizeof(cmd), "at+wifi_conf=%s,%s,%s", ssid, sec_str, password);
    HLK_LOG("wifi_conf: %s\n", cmd);
    int ret = hlk_rm58s_execute_at(cmd, NULL, 0, 0);
    HLK_LOG("wifi_conf returned: %d\n", ret);
    return ret;
}

int hlk_rm58s_config_tcp(const char *ip, uint16_t port) {
    if (!ip) return -1;

    char cmd[HLK_CMD_BUF_SIZE];

    if (hlk_rm58s_execute_at("at+dhcpc=1", NULL, 0, 0) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "at+remoteip=%s", ip);
    if (hlk_rm58s_execute_at(cmd, NULL, 0, 0) != 0) return -1;

    snprintf(cmd, sizeof(cmd), "at+remoteport=%d", port);
    if (hlk_rm58s_execute_at(cmd, NULL, 0, 0) != 0) return -1;

    if (hlk_rm58s_execute_at("at+remotepro=tcp", NULL, 0, 0) != 0) return -1;
    if (hlk_rm58s_execute_at("at+mode=client", NULL, 0, 0) != 0) return -1;

    return 0;
}

int hlk_rm58s_commit_and_reboot(void) {
    if (hlk_rm58s_execute_at("at+net_commit=1", NULL, 0, 0) != 0) {
        return -1;
    }
    hlk_rm58s_send_at("at+reconn=1");
    hlk_delay_ms(3000);
    hlk_set_state(HLK_STATE_IDLE);
    return 0;
}

// ===========================
// 状态查询
// ===========================

bool hlk_rm58s_is_wifi_connected(void) {
    hlk_update_state();
    return g_hlk.wifi_connected;
}

bool hlk_rm58s_is_tcp_connected(void) {
    hlk_update_state();
    return g_hlk.tcp_connected;
}

int hlk_rm58s_get_rssi(void) {
    char response[64];
    if (hlk_rm58s_execute_at("at+rssi=?", response, sizeof(response), 0) != 0) {
        return -1;
    }
    char *p = strstr(response, "-");
    if (p) return atoi(p);
    return -1;
}

int hlk_rm58s_get_version(char *buf, uint16_t size) {
    if (!buf || size == 0) return -1;
    return hlk_rm58s_execute_at("at+ver=?", buf, size, 0);
}

// ===========================
// 透传收发
// ===========================

int hlk_rm58s_send(uint8_t *data, uint16_t len) {
    if (!data || len == 0 || !g_hlk.uart_handle) return -1;
    if (g_hlk.state != HLK_STATE_TRANSPARENT) return -1;

    if (hal_uart_send(g_hlk.uart_handle, data, len) != 0) {
        return -1;
    }
    return len;
}

int hlk_rm58s_recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms) {
    if (!buf || max_len == 0) return -1;

    uint32_t start = hlk_get_tick();
    uint32_t timeout = (timeout_ms > 0) ? timeout_ms : 1000;

    while (!g_hlk.rx_complete) {
        if (hlk_get_tick() - start > timeout) {
            return 0;
        }
        hlk_delay_ms(10);
    }

    uint16_t len = g_hlk.rx_len;
    if (len > max_len) len = max_len;
    memcpy(buf, g_hlk.rx_buf, len);
    g_hlk.rx_complete = false;
    g_hlk.rx_len = 0;

    hlk_uart_start_recv();
    return len;
}

// ===========================
// 回调注册
// ===========================

void hlk_rm58s_register_rx_callback(hlk_rx_callback_t cb) {
    g_hlk.rx_callback = cb;
}

void hlk_rm58s_register_state_callback(hlk_state_callback_t cb) {
    g_hlk.state_callback = cb;
}

// ===========================
// 轮询
// ===========================

void hlk_rm58s_poll(void) {
    hlk_update_state();

    if (g_hlk.at_waiting) {
        if (hlk_get_tick() - g_hlk.at_start_tick > g_hlk.at_timeout_ms) {
            g_hlk.at_waiting = false;
        }
    }
}

// ===========================
// UART中断回调
// ===========================

void hlk_rm58s_uart_rx_complete(uint8_t *data, uint16_t len) {
    if (g_hlk.state == HLK_STATE_TRANSPARENT) {
        if (g_hlk.rx_callback) {
            g_hlk.rx_callback(data, len);
        }
    } else if (g_hlk.state == HLK_STATE_AT_MODE) {
        if (g_hlk.at_resp_len + len < sizeof(g_hlk.at_response)) {
            memcpy(g_hlk.at_response + g_hlk.at_resp_len, data, len);
            g_hlk.at_resp_len += len;
            g_hlk.at_response[g_hlk.at_resp_len] = '\0';

            if (str_contains(g_hlk.at_response, "OK") ||
                str_contains(g_hlk.at_response, "ERROR") ||
                str_contains(g_hlk.at_response, "ok") ||
                str_contains(g_hlk.at_response, "error")) {
                g_hlk.at_waiting = false;
            }
        }
    }
}

void hlk_rm58s_uart_tx_complete(void) {
    hlk_uart_start_recv();
}