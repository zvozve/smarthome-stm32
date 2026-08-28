/**
 * @file    task_ir.c
 * @brief   红外控制任务实现
 * @version V1.0
 * @date    2026-08-25
 */

#include "task_ir.h"
#include "board_cfg.h"     /* 工程侧绑定：IR_RX 引脚/TIM、IR_TX 配置 */
#include "ir_1838b.h"
#include "ir_tx.h"
#include "SEGGER_RTT_Log.h"
#include <string.h>

/* ========== 静态变量 ========== */

static bool             g_initialized   = false;
static TaskHandle_t     g_task_handle   = NULL;
static osMutexId_t      g_mutex         = NULL;

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

static void update_recv_data(const ir_raw_frame_t *raw)
{
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }

    if (raw != NULL && raw->valid) {
        memcpy(&g_recv_data.raw, raw, sizeof(ir_raw_frame_t));
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
    /*
     * 红外发射驱动已就绪（ir_tx.h，TIM9 38kHz 载波），接入流程：
     * 1. 解析 g_send_buf 内容（MQTT 下发的语义指令，如 "26度/制冷/自动风"）
     * 2. 根据指令选择对应空调协议编码，或取出学习阶段保存的 raw 时序
     * 3. 调用重放接口发出波形，例如：
     *      IR_TX_SendRaw(raw.timing_us, raw.edges, raw.level_start, 1, 0);
     */

    SYS_LOG("[IR] Send request received, len=%u (codec not implemented yet)", g_send_len);

    g_send_req = false;
    g_send_len = 0;
}

/* ========== 任务入口 ========== */

static void task_ir_entry(void *arg)
{
    (void)arg;

    ir_raw_frame_t frame;

    /* 初始化 1838B：注入引脚 + 1ms 节拍 TIM 句柄（TIM6, CubeMX 配 84MHz/84/1000） */
    if (!IR1838B_Init(BOARD_IR_RX_PORT, BOARD_IR_RX_PIN, BOARD_IR_RX_TIM)) {
        DBG_LOG("[IR] 1838B init failed");
        vTaskDelete(NULL);
        return;
    }

    /* 初始化红外发射（按 board_cfg 注入引脚/TIM9/载波参数） */
    static const ir_tx_cfg_t s_ir_tx_cfg = BOARD_IR_TX_CFG;
    if (!IR_TX_Init(&s_ir_tx_cfg)) {
        DBG_LOG("[IR] TX init failed");
    }

    DBG_LOG("[IR] Task started, waiting IR signal...");

    while (1) {
        if (IR1838B_Available()) {
            if (IR1838B_GetRaw(&frame)) {
                update_recv_data(&frame);
                IR1838B_PrintRaw(&frame);
            }
        }

        process_send_request();

        /* 每 2 秒打印一次状态 */
        if (++status_cnt >= (2000 / IR_TASK_PERIOD_MS)) {
            status_cnt = 0;
            IR1838B_PrintStatus();
        }

        vTaskDelay(pdMS_TO_TICKS(IR_TASK_PERIOD_MS));
    }
}

/* ========== 对外 API ========== */

bool Task_IR_Init(void)
{
    g_mutex = osMutexNew(NULL);
    if (g_mutex == NULL) {
        DBG_LOG("[IR] Mutex create failed");
    }

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
        /* 上一次还没处理完 */
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
    uint8_t  level_start = 0;
    uint16_t edges;

    if (state == NULL) {
        return false;
    }

    /* 编码（互斥保护静态缓冲，允许多任务调用） */
    if (g_mutex != NULL) {
        osMutexAcquire(g_mutex, osWaitForever);
    }
    edges = AC_Codec_Encode(brand, state, s_timing, AC_CODEC_MAX_EDGES, &level_start);
    if (edges == 0) {
        if (g_mutex != NULL) {
            osMutexRelease(g_mutex);
        }
        DBG_LOG("[IR] Encode failed, brand=%d", (int)brand);
        return false;
    }

    /* 发射：载波由 TIM9 硬件产生，此处按原始时序门控 */
    bool ok = IR_TX_SendRaw(s_timing, edges, level_start, 1, 0);
    if (g_mutex != NULL) {
        osMutexRelease(g_mutex);
    }

    DBG_LOG("[IR] SendAc brand=%d pwr=%d mode=%d temp=%u fan=%d -> edges=%u %s",
            (int)brand, state->power, (int)state->mode,
            state->temp_c, (int)state->fan, edges, ok ? "OK" : "BUSY/FAIL");
    return ok;
}