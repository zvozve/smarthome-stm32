#ifndef __HLK_RM58S_AT_H
#define __HLK_RM58S_AT_H

#include "hlk_rm58s_config.h"

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

// ===========================
// WiFi 模式枚举
// ===========================
typedef enum { 
    WIFI_MODE_STA, 
    WIFI_MODE_AP 
} WiFiMode_t;

#endif // __HLK_RM58S_AT_H