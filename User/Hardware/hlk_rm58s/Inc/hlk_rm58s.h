#ifndef __HLK_RM58S_H
#define __HLK_RM58S_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===========================
// RTT 日志宏（统一封装）
// ===========================

#ifndef HLK_LOG_ENABLE
    #define HLK_LOG_ENABLE     1
#endif

#if HLK_LOG_ENABLE
    #include "SEGGER_RTT.h"
    #define HLK_LOG(fmt, ...)   SEGGER_RTT_printf(0, "[HLK] " fmt, ##__VA_ARGS__)
#else
    #define HLK_LOG(fmt, ...)   ((void)0)
#endif

// ===========================
// 类型定义
// ===========================

typedef enum {
    HLK_STATE_UNINIT = 0,
    HLK_STATE_IDLE,
    HLK_STATE_AT_MODE,
    HLK_STATE_TRANSPARENT,
    HLK_STATE_WIFI_CONNECTING,
    HLK_STATE_WIFI_CONNECTED,
    HLK_STATE_TCP_CONNECTED,
    HLK_STATE_ERROR,
} hlk_state_t;

typedef enum {
    HLK_SECURITY_NONE = 0,
    HLK_SECURITY_WEP,
    HLK_SECURITY_WPA_PSK,
    HLK_SECURITY_WPA2_PSK,
    HLK_SECURITY_WPA_WPA2_PSK,
} hlk_security_t;

// ===========================
// 回调函数类型
// ===========================

typedef void (*hlk_rx_callback_t)(uint8_t *data, uint16_t len);
typedef void (*hlk_state_callback_t)(bool wifi_connected, bool tcp_connected);
typedef void (*hlk_delay_ms_t)(uint32_t ms);
typedef uint32_t (*hlk_get_tick_t)(void);

// ===========================
// 配置结构体
// ===========================

typedef struct {
    // 硬件句柄
    void *uart_handle;

    // 回调函数
    hlk_rx_callback_t rx_callback;
    hlk_state_callback_t state_callback;

    // 超时配置
    uint32_t at_timeout_ms;
    uint32_t connect_timeout_ms;
} hlk_config_t;

// ===========================
// 核心API
// ===========================

int  hlk_rm58s_init(hlk_config_t *config);
void hlk_rm58s_deinit(void);

void hlk_rm58s_reset(uint32_t hold_ms);
void hlk_rm58s_factory_reset(uint32_t wait_s);

void hlk_rm58s_enter_at_mode(uint32_t hold_ms);
void hlk_rm58s_exit_transparent_mode(void);
hlk_state_t hlk_rm58s_get_state(void);

int  hlk_rm58s_connect_sta(const char *ssid, const char *password, hlk_security_t security);
int  hlk_rm58s_config_tcp(const char *ip, uint16_t port);
int  hlk_rm58s_commit_and_reboot(void);

bool hlk_rm58s_is_wifi_connected(void);
bool hlk_rm58s_is_tcp_connected(void);
int  hlk_rm58s_get_rssi(void);
int  hlk_rm58s_get_version(char *buf, uint16_t size);

int  hlk_rm58s_execute_at(const char *cmd, char *response, uint16_t resp_size, uint32_t timeout_ms);
int  hlk_rm58s_send_at(const char *cmd);

int  hlk_rm58s_send(uint8_t *data, uint16_t len);
int  hlk_rm58s_recv(uint8_t *buf, uint16_t max_len, uint32_t timeout_ms);

void hlk_rm58s_register_rx_callback(hlk_rx_callback_t cb);
void hlk_rm58s_register_state_callback(hlk_state_callback_t cb);

uint32_t hlk_rm58s_get_tick(void);
void hlk_rm58s_poll(void);

// ===========================
// HAL层接口（由hlk_rm58s_hal.c实现）
// ===========================

// UART
int  hal_uart_send(void *handle, uint8_t *data, uint16_t len);
int  hal_uart_recv_start(void *handle, uint8_t *buf, uint16_t len);
void hal_uart_abort(void *handle);

// GPIO便捷函数
void hal_gpio_write_es(uint8_t level);
void hal_gpio_write_rst(uint8_t level);
uint8_t hal_gpio_read_sts(void);
uint8_t hal_gpio_read_sot(void);

// ===========================
// UART中断回调
// ===========================

void hlk_rm58s_uart_rx_complete(uint8_t *data, uint16_t len);
void hlk_rm58s_uart_tx_complete(void);

#ifdef __cplusplus
}
#endif

#endif /* __HLK_RM58S_H */