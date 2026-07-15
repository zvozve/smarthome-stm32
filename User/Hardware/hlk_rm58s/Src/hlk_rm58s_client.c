#include "hlk_rm58s_client.h"
#include "hlk_rm58s_manager.h"
#include "hlk_rm58s_config.h"
#include "hlk_rm58s_port.h"
#include "hlk_rm58s_driver.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

// ===========================
// 私有数据
// ===========================
static struct {
    hlk_rx_callback_t rx_callback;
    hlk_status_callback_t status_callback;
    hlk_status_t last_status;
    char jsonTxBuf[HLK_UART_BUF_LEN];
    char jsonRxBuf[HLK_UART_BUF_LEN];
} client_ctx = {
    .rx_callback = NULL,
    .status_callback = NULL,
    .last_status = HLK_IDLE,
};

// ===========================
// 内部函数声明
// ===========================
static void client_status_changed(hlk_status_t new_status);
void HLK_Client_RxHandler(const char* data, uint16_t len);  // 添加声明

// ===========================
// Client 初始化
// ===========================
void HLK_Client_Init(void) {
    HLK_LOG("HLK_DRIVER_VERSION: %s\r\n", HLK_DRIVER_VERSION);

    // 初始化Manager（状态机）
    HLK_Manager_Init();
    
    // 注册Manager的接收回调
    HLK_Manager_RegisterRxCallback(HLK_Client_RxHandler);
    
    // 初始状态
    client_ctx.last_status = HLK_IDLE;
}

// ===========================
// Client 任务（用户周期性调用）
// ===========================
void HLK_Client_Task(void) {
    hlk_status_t current_status;
    
    // 1. 运行Manager状态机
    HLK_Manager_Task();
    
    // 2. 检查状态变化
    current_status = HLK_Client_GetStatus();
    if (current_status != client_ctx.last_status) {
        client_status_changed(current_status);
        client_ctx.last_status = current_status;
    }
    
    // 3. 如果未就绪，不执行业务
    if (!HLK_Client_IsReady()) {
        return;
    }

    // 4. 执行业务：发送JSON（示例）
    HLK_Client_BuildJSON(client_ctx.jsonTxBuf, sizeof(client_ctx.jsonTxBuf));
    HLK_Client_SendString(client_ctx.jsonTxBuf);
    HLK_LOG("Send JSON: %s\n", client_ctx.jsonTxBuf);
}

// ===========================
// 接收数据处理（由Manager回调）
// ===========================
void HLK_Client_RxHandler(const char* data, uint16_t len) {
    if (!data || len == 0) return;
    
    // 复制数据
    uint16_t copy_len = (len > HLK_UART_BUF_LEN - 1) ? HLK_UART_BUF_LEN - 1 : len;
    memcpy(client_ctx.jsonRxBuf, data, copy_len);
    client_ctx.jsonRxBuf[copy_len] = '\0';
    
    // 解析JSON
    HLK_Client_ParseJSON(client_ctx.jsonRxBuf);
    HLK_LOG("Recv JSON: %s\n", client_ctx.jsonRxBuf);
    
    // 调用用户回调
    if (client_ctx.rx_callback) {
        client_ctx.rx_callback(data, len);
    }
}

// ===========================
// 状态变化处理
// ===========================
static void client_status_changed(hlk_status_t new_status) {
    HLK_LOG("Status changed: %d -> %d\n", client_ctx.last_status, new_status);
    
    if (client_ctx.status_callback) {
        client_ctx.status_callback(new_status);
    }
}

// ===========================
// Client API 实现
// ===========================
bool HLK_Client_Send(const char* data, uint16_t len) {
    if (!HLK_Client_IsReady()) return false;
    if (!data || len == 0) return false;
    HLK_LOG("HLK_Client_Send");
    HLK_Driver_Transmit(data);
    return true;
}

bool HLK_Client_SendString(const char* str) {
    if (!str) return false;
    return HLK_Client_Send(str, strlen(str));
}

hlk_status_t HLK_Client_GetStatus(void) {
    mgr_status_t mgr_status = HLK_Manager_GetStatus();
    return (hlk_status_t)mgr_status;
}

bool HLK_Client_IsReady(void) {
    return HLK_Manager_IsReady();
}

void HLK_Client_SetRxCallback(hlk_rx_callback_t cb) {
    client_ctx.rx_callback = cb;
}

void HLK_Client_SetStatusCallback(hlk_status_callback_t cb) {
    client_ctx.status_callback = cb;
}

void HLK_Client_Reconnect(void) {
    HLK_Manager_Reconfig();
}

// ===========================
// JSON 工具
// ===========================
void HLK_Client_BuildJSON(char* buf, size_t buf_size) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device", "rm58s");
    cJSON_AddNumberToObject(root, "timestamp", HAL_GetTick());
    cJSON_AddStringToObject(root, "status", "ok");
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        strncpy(buf, json_str, buf_size - 1);
        buf[buf_size - 1] = '\0';
        free(json_str);
    }
    
    cJSON_Delete(root);
}

void HLK_Client_ParseJSON(char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    
    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cJSON_IsString(cmd)) {
        HLK_LOG("Received cmd: %s\n", cmd->valuestring);
    }
    
    cJSON_Delete(root);
}