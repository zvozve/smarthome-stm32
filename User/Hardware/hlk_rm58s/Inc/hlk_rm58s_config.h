#ifndef __HLK_RM58S_CONFIG_H
#define __HLK_RM58S_CONFIG_H

#include "network_config.h"

// ===========================
// WiFi STA 模式配置
// ===========================
// #define WIFI_SSID          "USIDE-2F"
// #define WIFI_PASSWORD      "uside8888"
// #define WIFI_SECURITY      "wpa2_aes"

// // ===========================
// // 远程服务器
// // ===========================
// #define REMOTE_IP          "192.168.31.144"
// #define REMOTE_PORT        "9998"
// #define PROTOCOL           "tcp"

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
// #define AP_GATEWAY         "192.168.31.254"
// #define AP_NETMASK         "255.255.255.0"

// ===========================
// 超时
// ===========================
#define RESPONSE_TIMEOUT    2000

// ===========================
// AT 指令
// ===========================
#define AT_NETMODE_STA     "at+netmode=2"
#define AT_NETMODE_AP      "at+netmode=3"
#define AT_WIFI_CONF       "at+wifi_conf=" WIFI_SSID "," WIFI_SECURITY "," WIFI_PASSWORD
#define AT_DHCPC_ON        "at+dhcpc=1"
#define AT_DHCPC_OFF       "at+dhcpc=0"
#define AT_REMOTEIP        "at+remoteip=" REMOTE_IP
#define AT_REMOTEPORT      "at+remoteport=" REMOTE_PORT
#define AT_REMOTEPRO       "at+remotepro=" PROTOCOL
#define AT_MODE_CLIENT     "at+mode=client"
#define AT_MODE_SERVER     "at+mode=server"
#define AT_UART            "at+uart=" UART_BAUDRATE "," UART_DATABITS "," UART_PARITY "," UART_STOPBITS
#define AT_UARTPACKLEN     "at+uartpacklen=" UART_PACKLEN
#define AT_UARTPACKTIMEOUT "at+uartpacktimeout=" UART_PACKTIMEOUT
#define AT_NET_COMMIT      "at+net_commit=1"
#define AT_RECONN          "at+reconn=1"
#define AT_DEFAULT         "at+default=1"
#define AT_REBOOT          "at+reboot=1"
#define AT_QUERY_MAC       "at+Get_MAC=?"

#endif // __HLK_RM58S_CONFIG_H