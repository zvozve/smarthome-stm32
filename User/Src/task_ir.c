/**
 * @file    task_ir.c
 * @brief   红外控制任务实现（接收：SDK ir_receiver；发射：SDK ir_transmitter）
 * @version V2.0
 * @date    2026-09-21
 */

#include "task_ir.h"
#include "board_cfg.h"     /* IR 定时器绑定宏 */
#include "ir_receiver.h"
#include "ir_transmitter.h"
#include "ac_codec.h"
#include "SEGGER_RTT_Log.h"
#include <string.h>

/* ========== 静态变量 ========== */

static bool             g_initialized   = false;
static TaskHandle_t     g_task_handle   = NULL;
static osMutexId_t      g_mutex         = NULL;

/* 接收实例 + 时长缓冲（缓冲由 board_cfg 的 BOARD_IR_RX_BUF_SIZE 决定） */
static uint16_t         s_rx_buf[BOARD_IR_RX_BUF_SIZE];
static ir_receiver_t    g_ir_rx;
static ir_transmitter_t g_ir_tx;

/* 接收数据缓存 */
static IR_RecvData_t    g_recv_data = {
    .has_new    = false,
    .timestamp  = 0
};

/* 发送请求预留 */
static volatile bool    g_send_req      = false;
static uint8_t          g_send_buf[64];
static uint16_t         g_send_len      = 0;

static uint32_t status_cnt = 0;

/* ========== 内部函数 ========== */

/* 把 SDK 接收驱动刚收完的一帧拷贝到线程安全的缓存 */
static void update_recv_data(void)
{
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }

    uint16_t cnt = ir_receiver_count(&g_ir_rx);
    if (cnt > 0U) {
        const uint16_t *src = ir_receiver_data(&g_ir_rx);
        uint16_t n = (cnt > BOARD_IR_RX_BUF_SIZE) ? BOARD_IR_RX_BUF_SIZE : cnt;
        memcpy(g_recv_data.timings, src, n * sizeof(uint16_t));
        g_recv_data.count     = n;
        g_recv_data.has_new   = true;
        g_recv_data.timestamp = HAL_GetTick();
    }

    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }
}

static void process_send_request(void)
{
    if (!g_send_req) {
        return;
    }

    /* ========== 待接入：MQTT 指令 → 空调红外编码 ========== */
    SYS_LOG("[IR] Send request received, len=%u (codec not implemented yet)", g_send_len);

    g_send_req = false;
    g_send_len = 0;
}

/* ========== 对外 API ========== */

/* 由 Core 的 HAL_TIM_IC_CaptureCallback 调用 */
void task_ir_rx_isr(void)
{
    ir_receiver_isr(&g_ir_rx);
}

static void task_ir_entry(void *arg)
{
    (void)arg;

    DBG_LOG("[IR] Task started, waiting IR signal...");

    while (1) {
        /* 一帧收完（ir_receiver 内部已自动停捕获），取出数据并重启下一帧 */
        if (ir_receiver_process(&g_ir_rx)) {
            update_recv_data();
            DBG_LOG("[IR] frame received, edges=%u", (unsigned)ir_receiver_count(&g_ir_rx));
            (void)ir_receiver_start(&g_ir_rx);
        }

        process_send_request();

        /* 每 2 秒打印一次状态 */
        if (++status_cnt >= (2000 / IR_TASK_PERIOD_MS)) {
            status_cnt = 0;
            DBG_LOG("[IR] alive");
        }

        vTaskDelay(pdMS_TO_TICKS(IR_TASK_PERIOD_MS));
    }
}

bool Task_IR_Init(void)
{
    g_mutex = osMutexNew(NULL);
    if (g_mutex == NULL) {
        DBG_LOG("[IR] Mutex create failed");
    }

    /* 设备初始化收口到 Task_IR_Init（任务体内只跑逻辑） */
    static const ir_receiver_cfg_t s_rx_cfg = {
        .htim        = BOARD_IR_RX_TIM,
        .channel     = BOARD_IR_RX_CHANNEL,
        .clk_hz      = BOARD_IR_RX_CLK_HZ,
        .buf         = s_rx_buf,
        .cap         = BOARD_IR_RX_BUF_SIZE,
        .idle_us     = BOARD_IR_RX_IDLE_US,
        .debounce_us = BOARD_IR_RX_DEBOUNCE_US,
    };
    if (!ir_receiver_init(&g_ir_rx, &s_rx_cfg)) {
        DBG_LOG("[IR] receiver init failed");
        return false;
    }
    if (!ir_receiver_start(&g_ir_rx)) {
        DBG_LOG("[IR] receiver start failed");
        return false;
    }
    /* 发射在 Task_IR_SendAc 内按需初始化（收/发已分到不同定时器，互不干扰） */

    if (xTaskCreate(task_ir_entry,
                    IR_TASK_NAME,
                    IR_TASK_STACK_SIZE,
                    NULL,
                    IR_TASK_PRIORITY,
                    &g_task_handle) != pdPASS) {
        DBG_LOG("[IR] Task create failed");
        return false;
    }

    g_initialized = true;
    DBG_LOG("[IR] Task created");
    return true;
}

bool Task_IR_GetRecvData(IR_RecvData_t *pData)
{
    if (pData == NULL || !g_initialized) {
        return false;
    }

    bool has_new = false;

    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }

    if (g_recv_data.has_new) {
        memcpy(pData, &g_recv_data, sizeof(IR_RecvData_t));
        has_new = true;
    }

    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }

    return has_new;
}

void Task_IR_ClearNewFlag(void)
{
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }

    g_recv_data.has_new = false;

    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }
}

bool Task_IR_SendByMQTT(const uint8_t *cmd, uint16_t cmd_len)
{
    if (cmd == NULL || cmd_len == 0 || cmd_len > sizeof(g_send_buf)) {
        return false;
    }

    if (g_send_req) {
        return false;
    }

    memcpy(g_send_buf, cmd, cmd_len);
    g_send_len = cmd_len;
    g_send_req = true;

    return true;
}

bool Task_IR_SendAc(ac_brand_t brand, const ac_state_t *state)
{
    static uint16_t s_timing[AC_CODEC_MAX_EDGES];
    uint16_t edges;

    if (state == NULL) {
        return false;
    }

    /* 编码（互斥保护静态缓冲，允许多任务调用） */
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }
    edges = AC_Codec_Encode(brand, state, s_timing, AC_CODEC_MAX_EDGES, NULL);
    if (edges == 0) {
        if (g_mutex != NULL) {
            osMutexRelease(g_mutex);
        }
        DBG_LOG("[IR] Encode failed, brand=%d", (int)brand);
        return false;
    }

    /* 发射：SDK ir_transmitter 自配 TIM3 PWM 载波 + TIM6 µs 时基。
     * 收/发已分到不同定时器（接收=TIM9 / 发射=TIM3），无需收发间重配。 */
    static const ir_transmitter_cfg_t s_tx_cfg = {
        .htim             = BOARD_IR_TX_TIM,
        .channel          = BOARD_IR_TX_CHANNEL,
        .tim_clk_hz       = BOARD_IR_TX_TIM_CLK_HZ,
        .carrier_hz       = BOARD_IR_TX_CARRIER_HZ,
        .duty_percent     = BOARD_IR_TX_DUTY,
        .inverted         = BOARD_IR_TX_INVERTED,
        .use_carrier      = BOARD_IR_TX_USE_CARRIER,
        .tick_htim        = BOARD_IR_TICK_TIM,
        .tick_tim_clk_hz  = BOARD_IR_TICK_CLK_HZ,
    };

    bool ok = false;
    if (ir_transmitter_init(&g_ir_tx, &s_tx_cfg)) {
        ok = ir_transmitter_send(&g_ir_tx, s_timing, edges);
        ir_transmitter_deinit(&g_ir_tx);
    }

    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }

    DBG_LOG("[IR] SendAc brand=%d pwr=%d mode=%d temp=%u fan=%d -> edges=%u %s",
            (int)brand, state->power, (int)state->mode,
            state->temp_c, (int)state->fan, edges, ok ? "OK" : "BUSY/FAIL");
    return ok;
}
