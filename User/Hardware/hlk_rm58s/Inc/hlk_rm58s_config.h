#ifndef __HLK_RM58S_CONFIG_H
#define __HLK_RM58S_CONFIG_H

// ===========================
// 驱动内部配置（用户一般不需要修改）
// ===========================

#define HLK_RX_BUF_SIZE         1024
#define HLK_AT_RESP_BUF_SIZE    512
#define HLK_CMD_BUF_SIZE        256

#define HLK_AT_TIMEOUT_DEFAULT      2000
#define HLK_CONNECT_TIMEOUT_DEFAULT 10000
#define HLK_RESET_HOLD_MS           600
#define HLK_AT_ENTER_HOLD_MS        1200

#endif /* __HLK_RM58S_CONFIG_H */