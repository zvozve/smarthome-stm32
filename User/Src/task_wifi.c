/**
 * @file    task_wifi.c
 * @brief   WiFi 链路任务（轮询版）
 */

#include "task_wifi.h"
#include "board_cfg.h"     /* 工程侧绑定：HLK UART + 控制引脚 */
#include "hlk_rm58s.h"
#include "oop_gpio_drv.h"
#include "oop_uart_drv.h"
#include "oop_dwt.h"
#include "SEGGER_RTT_Log.h"
#include "FreeRTOS.h"
#include "task.h"
#include "config_network.h"

/* ===========================
 * 任务配置
 * =========================== */
#define WIFI_TASK_STACK_SIZE    4096
#define WIFI_TASK_PRIORITY      3
#define WIFI_TASK_NAME          "WiFiTask"

/* ===========================
 * WiFi 参数
 * 说明：SSID/密码/加密方式/远端IP/端口/协议均来自 config_network.h，
 *       此处不再硬编码，改网络配置只动 config_network.h 一处。
 * =========================== */
#define HLK_UART_BAUDRATE       115200
#define HLK_UART_DATABITS       8
#define HLK_UART_PARITY         'n'
#define HLK_UART_STOPBITS       1
#define HLK_UART_PACK_LEN       512
#define HLK_UART_PACK_TIMEOUT_MS 20
#define HLK_DHCP_ENABLE         true
#define HLK_AUTO_RECONNECT      true
#define HLK_RESPONSE_TIMEOUT_MS 3000
#define HLK_MAX_RETRY           3

/* ===========================
 * 回调注册表
 * =========================== */
#define WIFI_MAX_RX_CALLBACKS       4
#define WIFI_MAX_STATE_CALLBACKS    4

/* ===========================
 * BSP 硬件设备
 * =========================== */
static gpio_dev_t   g_gpio_es;
static gpio_dev_t   g_gpio_rst;
static gpio_dev_t   g_gpio_sts;
static gpio_dev_t   g_gpio_sot;
static uart_drv_t   g_uart_hlk;

/* ===========================
 * 状态
 * =========================== */
static volatile bool    g_connected = false;
static volatile bool    g_initialized = false;
static hlk_drv_t       *g_hlk = NULL;

/* ===========================
 * 回调注册表
 * =========================== */
static struct {
    wifi_rx_callback_t  cb;
    void               *ctx;
    bool                active;
} g_rx_callbacks[WIFI_MAX_RX_CALLBACKS];

static struct {
    wifi_state_callback_t cb;
    void                 *ctx;
    bool                  active;
} g_state_callbacks[WIFI_MAX_STATE_CALLBACKS];

/* ===========================
 * 内部函数声明
 * =========================== */
static void task_wifi_entry(void *arg);
static void wifi_hardware_init(void);
static hlk_params_t wifi_build_params(void);
static void wifi_dispatch_rx(const uint8_t *data, uint16_t len);
static void wifi_dispatch_state(bool connected);
static void wifi_on_rx(uint8_t *data, uint16_t len, void *ctx);
static void wifi_on_state(hlk_state_t state, void *ctx);

/* ===========================
 * 构建配置参数
 * =========================== */
static hlk_params_t wifi_build_params(void) {
    hlk_params_t p = {
        .wifi_ssid          = WIFI_SSID,
        .wifi_password      = WIFI_PASSWORD,
        .wifi_security      = WIFI_SECURITY,
        .remote_ip          = REMOTE_IP,
        .remote_port        = REMOTE_PORT,
        .remote_protocol    = PROTOCOL,
        .uart_baudrate      = HLK_UART_BAUDRATE,
        .uart_databits      = HLK_UART_DATABITS,
        .uart_parity        = HLK_UART_PARITY,
        .uart_stopbits      = HLK_UART_STOPBITS,
        .uart_pack_len      = HLK_UART_PACK_LEN,
        .uart_pack_timeout_ms = HLK_UART_PACK_TIMEOUT_MS,
        .dhcp_enable        = HLK_DHCP_ENABLE,
        .auto_reconnect     = HLK_AUTO_RECONNECT,
        .response_timeout_ms = HLK_RESPONSE_TIMEOUT_MS,
        .max_retry          = HLK_MAX_RETRY,
    };
    return p;
}

/* ===========================
 * 硬件初始化
 * =========================== */
static void wifi_hardware_init(void) {
    oop_gpio_init_output(&g_gpio_es,   BOARD_HLK_ES_PORT,  BOARD_HLK_ES_PIN,  true);
    oop_gpio_init_output(&g_gpio_rst,  BOARD_HLK_RST_PORT, BOARD_HLK_RST_PIN, true);
    oop_gpio_init_input(&g_gpio_sts, BOARD_HLK_STS_PORT, BOARD_HLK_STS_PIN, true);
    oop_gpio_init_input(&g_gpio_sot, BOARD_HLK_SOT_PORT, BOARD_HLK_SOT_PIN, true);

    uart_drv_init(&g_uart_hlk, BOARD_HLK_UART, NULL);

    DBG_LOG("WiFi: Hardware init done");
}

/* ===========================
 * 回调分发
 * =========================== */
static void wifi_dispatch_rx(const uint8_t *data, uint16_t len) {
    for (int i = 0; i < WIFI_MAX_RX_CALLBACKS; i++) {
        if (g_rx_callbacks[i].active && g_rx_callbacks[i].cb != NULL) {
            g_rx_callbacks[i].cb(data, len, g_rx_callbacks[i].ctx);
        }
    }
}

static void wifi_dispatch_state(bool connected) {
    for (int i = 0; i < WIFI_MAX_STATE_CALLBACKS; i++) {
        if (g_state_callbacks[i].active && g_state_callbacks[i].cb != NULL) {
            g_state_callbacks[i].cb(connected, g_state_callbacks[i].ctx);
        }
    }
}

/* ===========================
 * HLK 驱动回调
 * =========================== */
static void wifi_on_rx(uint8_t *data, uint16_t len, void *ctx) {
    (void)ctx;
    if (data == NULL || len == 0) return;
    wifi_dispatch_rx(data, len);
}

static void wifi_on_state(hlk_state_t state, void *ctx) {
    (void)ctx;
    bool connected = (state == HLK_STATE_LINKED);
    if (connected != g_connected) {
        g_connected = connected;
        DBG_LOG("WiFi: %s", connected ? "CONNECTED" : "DISCONNECTED");
        wifi_dispatch_state(connected);
    }
}

/* ===========================
 * ★ WiFi 任务主循环（轮询版）★
 * =========================== */
static void task_wifi_entry(void *arg) {
    (void)arg;

    DBG_LOG("WiFi: tick_ms=%lu, cycle=%lu, core=%lu", 
        HLK_GET_TICK_MS(), oop_GetCycleCount(), SystemCoreClock);

    DBG_LOG("WiFi: Task started");

    /* 1. 硬件初始化 */
    wifi_hardware_init();

    /* 2. 创建 HLK 驱动实例 */
    hlk_params_t params = wifi_build_params();

    g_hlk = hlk_create(&g_gpio_es, &g_gpio_rst, &g_gpio_sts, &g_gpio_sot,
                       &g_uart_hlk, &params);
    if (g_hlk == NULL) {
        DBG_LOG("WiFi: Driver create failed");
        goto error_exit;
    }

    /* 3. 注册回调 */
    hlk_register_callbacks(g_hlk, wifi_on_rx, NULL, wifi_on_state, NULL);

    /* 4. 启动配网 */
    hlk_config(g_hlk);

    g_initialized = true;
    DBG_LOG("WiFi: Ready");

    /* 5. ★ 主循环：纯轮询，不阻塞 ★ */
    while (1) {
        hlk_process(g_hlk);
        vTaskDelay(pdMS_TO_TICKS(10));
    }

error_exit:
    while (1) {
        vTaskSuspend(NULL);
    }
}

/* ===========================
 * 对外 API
 * =========================== */

bool Task_WiFi_init(void) {
    if (xTaskCreate(task_wifi_entry, WIFI_TASK_NAME,
                    WIFI_TASK_STACK_SIZE, NULL,
                    WIFI_TASK_PRIORITY, NULL) != pdPASS) {
        DBG_LOG("WiFi: Task create failed");
        return false;
    }
    DBG_LOG("WiFi: Task created");
    return true;
}

bool Task_WiFi_is_connected(void) {
    return g_connected;
}

int Task_WiFi_send(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0) return -1;
    if (g_hlk == NULL) return -2;
    if (!g_connected) return -3;
    return hlk_send(g_hlk, data, len);
}

int Task_WiFi_register_rx_callback(wifi_rx_callback_t callback, void *ctx) {
    if (callback == NULL) return -1;
    for (int i = 0; i < WIFI_MAX_RX_CALLBACKS; i++) {
        if (g_rx_callbacks[i].active && g_rx_callbacks[i].cb == callback) {
            return 0;
        }
    }
    for (int i = 0; i < WIFI_MAX_RX_CALLBACKS; i++) {
        if (!g_rx_callbacks[i].active) {
            g_rx_callbacks[i].cb = callback;
            g_rx_callbacks[i].ctx = ctx;
            g_rx_callbacks[i].active = true;
            DBG_LOG("WiFi: RX callback registered");
            return 0;
        }
    }
    return -2;
}

int Task_WiFi_unregister_rx_callback(wifi_rx_callback_t callback) {
    if (callback == NULL) return -1;
    for (int i = 0; i < WIFI_MAX_RX_CALLBACKS; i++) {
        if (g_rx_callbacks[i].active && g_rx_callbacks[i].cb == callback) {
            g_rx_callbacks[i].active = false;
            g_rx_callbacks[i].cb = NULL;
            g_rx_callbacks[i].ctx = NULL;
            DBG_LOG("WiFi: RX callback unregistered");
            return 0;
        }
    }
    return -2;
}

int Task_WiFi_register_state_callback(wifi_state_callback_t callback, void *ctx) {
    if (callback == NULL) return -1;
    for (int i = 0; i < WIFI_MAX_STATE_CALLBACKS; i++) {
        if (g_state_callbacks[i].active && g_state_callbacks[i].cb == callback) {
            return 0;
        }
    }
    for (int i = 0; i < WIFI_MAX_STATE_CALLBACKS; i++) {
        if (!g_state_callbacks[i].active) {
            g_state_callbacks[i].cb = callback;
            g_state_callbacks[i].ctx = ctx;
            g_state_callbacks[i].active = true;
            DBG_LOG("WiFi: State callback registered");
            return 0;
        }
    }
    return -2;
}