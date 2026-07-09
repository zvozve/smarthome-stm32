#include "hlk_rm58s_driver.h"
#include "cJSON.h"

Uart_Task_t Uart_Task_WiFi = {
    .Port = &huart_hlk,
    .RS485_TX_Mode = NULL,
    .RS485_RX_Mode = NULL,
    .debug_name = "UART_HLK"
};

// ===========================
// IO 指令发送
// ===========================

// IO 初始化
void hlk_rm58s_gpio_init(void) {
    WIFI_ES_HIGH();
    WIFI_RST_HIGH();
    // WIFI_LINK_OFF();
    // WIFI_RX_TX_OFF();
}

// 进入 AT 模式
void hlk_rm58s_gpio_active_at_mode(void) {
    WIFI_ES_LOW();
    HAL_Delay(1200); 
    WIFI_ES_HIGH();
    HAL_Delay(200);
    SEGGER_RTT_printf(0, "hlk_rm58s_gpio_active_at_mode\n");
}

// 模块复位
void hlk_rm58s_gpio_avtive_reset(void) {
    WIFI_RST_LOW();
    HAL_Delay(600);
    WIFI_RST_HIGH();
    HAL_Delay(2000);
    SEGGER_RTT_printf(0, "hlk_rm58s_gpio_avtive_reset done!\n");
}

// 恢复出厂
void hlk_rm58s_gpio_active_recovery_mode(void) {
    WIFI_ES_LOW();
    for (int i = 9; i > 0; i--) {
        SEGGER_RTT_printf(0, "Wait %d s...\n", i);
        HAL_Delay(1000);
    }
    WIFI_ES_HIGH();
    HAL_Delay(1000);
    SEGGER_RTT_printf(0, "hlk_rm58s_gpio_active_recovery_mode done!\n");
}

// ===========================
// AT 指令发送
// ===========================

wifi_t wifiMaster = {0};
char jsonTxBuf[UART_BUF_LEN];
char jsonRxBuf[UART_BUF_LEN];
bool rx_flag = false;

void wifiMaster_cmd_clr(void) {    
    memset(&wifiMaster.Tx, 0, sizeof(wifiCommand_t));    
    memset(&wifiMaster.Rx, 0, sizeof(wifiCommand_t));   
}

void wifiMaster_transfer_block(uint16_t us) {
    while (Uart_Task_WiFi.status != UTS_IDLE) {
        TIM_HLK_Delay_us(us);
    }
}

void wifiMaster_transfer_at_cmd(const char *command, const char *expected_response, uint32_t timeout) {
    // 确保 AT 指令正确以 \r\n 结尾 (HLK-RM58S 规格书: 所有命令以 "at" 开始, "\r" 结束)
    char cmd_buf[128];
    size_t len = strlen(command);

    if (len > 0 && command[len - 1] == '\n') {
        // 已有 \r\n, 直接使用
        strncpy(cmd_buf, command, sizeof(cmd_buf) - 1);
        cmd_buf[sizeof(cmd_buf) - 1] = '\0';
    } else if (len > 0 && command[len - 1] == '\r') {
        // 已有 \r, 补 \n
        snprintf(cmd_buf, sizeof(cmd_buf), "%s\n", command);
        len += 1;
    } else {
        // 无换行, 补 \r\n
        snprintf(cmd_buf, sizeof(cmd_buf), "%s\r\n", command);
        len += 2;
    }

    wifiMaster_cmd_clr();

    wifiMaster.Tx.cmdByte = len;
    memcpy(wifiMaster.Tx.cmdBuf, cmd_buf, wifiMaster.Tx.cmdByte);

    Uart_Task_WiFi.status = UTS_REQUESTING;
    wifiMaster_transfer_block(TIM_HLK_PERIOD_US);
}

int wifiMaster_ParseResponse(const char *response, const char *expected) {
    if (!response) return 1;

    SEGGER_RTT_printf(0, "<< %s\n", response);

    // 匹配用户期望响应
    if (expected && strstr(response, expected)) return 0;

    // 自动识别 OK / ERROR / FAIL (大小写不敏感)
    if (strcasestr_local(response, "ok")) return 0;
    if (strcasestr_local(response, "error") || strcasestr_local(response, "fail")) return 1;

    // 未识别的情况也算异常
    return 1;
}

// 等待AT指令响应
// wifiMaster_transfer_at_cmd 内部已通过 wifiMaster_transfer_block 阻塞等待状态机完成
// 状态机由 TIM6 中断驱动: REQUESTING -> TX_ING -> TX_CPLT -> RX_ING -> RX_CPLT -> PARSING -> CALLBACK -> IDLE
// 返回 UTS_IDLE 后, wifiMaster.Rx.cmdBuf 中即为模块响应
int wifiMaster_WaitResponse(const char *expected, uint32_t timeout_ms) {
    (void)timeout_ms;  // 阻塞等待已在 transfer_at_cmd 中完成
    // transfer_at_cmd 已经阻塞等待到 UTS_IDLE, 这里直接解析结果
    if (wifiMaster.Rx.cmdByte == 0) {
        SEGGER_RTT_printf(0, "No response!\n");
        return 1;
    }
    return wifiMaster_ParseResponse((const char *)wifiMaster.Rx.cmdBuf, expected);
}

// 模块配置（对照HLK-RM58S规格书AT指令集）
// STA模式: 模块作为WiFi STA连接路由器, 同时作为TCP Client连接远端服务器
void hlk_rm58s_configure_WiFi_Module(void) {
    SEGGER_RTT_printf(0, "Start WiFi module config...\n");

    // STA模式配置序列 (对照规格书 4.2 节)
    const char *sta_cmds[] = {
        "at+netmode=2\r\n",                                    // 1. STA模式
        "at+wifi_conf=" WIFI_SSID "," WIFI_SECURITY "," WIFI_PASSWORD "\r\n",  // 2. SSID,加密,密码
        "at+dhcpc=1\r\n",                                      // 3. DHCP动态IP
        "at+remoteip=" REMOTE_IP "\r\n",                       // 4. 远端服务器IP
        "at+remoteport=" REMOTE_PORT "\r\n",                   // 5. 远端端口
        "at+remotepro=" PROTOCOL "\r\n",                       // 6. TCP协议
        "at+mode=client\r\n",                                  // 7. 客户端模式
        "at+uart=" UART_BAUDRATE "," UART_DATABITS "," UART_PARITY "," UART_STOPBITS "\r\n",  // 8. 串口参数
        "at+net_commit=1\r\n",                                 // 9. 提交参数
        NULL
        // at+reconn=1 单独处理: 模块会立即重启, 不返回ok
    };

    for (int i = 0; sta_cmds[i] != NULL; i++) {
        SEGGER_RTT_printf(0, ">> %s", sta_cmds[i]);
        wifiMaster_transfer_at_cmd(sta_cmds[i], "ok", RESPONSE_TIMEOUT);
        if (wifiMaster_WaitResponse("ok", 2000)) {
            SEGGER_RTT_printf(0, "Command failed: %s\n", sta_cmds[i]);
            return;
        }
        HAL_Delay(100);  // 指令间延时
    }

    // 重启模块 — 直接发送, 不经过状态机 (模块会立即重启, 不返回ok)
    SEGGER_RTT_printf(0, ">> at+reconn=1\r\n");
    Uart_Task_WiFi.status = UTS_IDLE;  // 确保状态机空闲
    HAL_UART_Transmit_DMA(&huart_hlk, (uint8_t*)"at+reconn=1\r\n", 13);
    HAL_Delay(1000);  // 等待发送完成 + 模块接收
    SEGGER_RTT_printf(0, "Module rebooting, wait 3s...\n");
    HAL_Delay(3000);

    SEGGER_RTT_printf(0, "WiFi module configured successfully\n");
}

// ===========================
// 字符串大小写无关匹配
// ===========================

char *strcasestr_local(const char *haystack, const char *needle) {
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

// ===========================
// JSON 指令发送
// ===========================

// ----------------- JSON 构建 -----------------
void build_json_message(char* buf, size_t buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", "rm58s");
    cJSON_AddNumberToObject(root, "timestamp", HAL_GetTick());
    cJSON_AddStringToObject(root, "status", "ok");

    char *json_str = cJSON_PrintUnformatted(root);
    strncpy(buf, json_str, buf_size - 1);
    buf[buf_size - 1] = '\0';

    cJSON_Delete(root);
    free(json_str);
}

// ----------------- JSON 解析 -----------------
void parse_json(char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cJSON_IsString(cmd)) {
        SEGGER_RTT_printf(0, "Received cmd: %s\n", cmd->valuestring);
    }

    cJSON_Delete(root);
}

// ===========================
// MEM
// ===========================


