#ifndef __HLK_RM58S_CONFIG_H
#define __HLK_RM58S_CONFIG_H

// ===========================
// WiFi STA 模式配置
// ===========================
#define WIFI_SSID          "USIDE-2F"
#define WIFI_PASSWORD      "uside8888"
#define WIFI_SECURITY      "wpa2_aes"

// ===========================
// 远程服务器
// ===========================
#define REMOTE_IP          "192.168.31.144"
#define REMOTE_PORT        "9998"
#define PROTOCOL           "tcp"

// ===========================
// 串口配置
// ===========================
#define UART_BAUDRATE      "115200"
#define UART_DATABITS      "8"
#define UART_PARITY        "n"
#define UART_STOPBITS      "1"
#define UART_PACKLEN       "512"
#define UART_PACKTIMEOUT   "20"
// ===========================
// AP模式配置（备选）
// ===========================
#define AP_GATEWAY         "192.168.31.254"
#define AP_NETMASK         "255.255.255.0"

#endif // __HLK_RM58S_CONFIG_H