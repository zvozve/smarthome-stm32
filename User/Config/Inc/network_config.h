/**
 * user_network.h — Centralized network configuration for SmartHome
 *
 * All WiFi, MQTT, IP, and port settings are managed here.
 * Include this header wherever network parameters are needed.
 */

#ifndef __NETWORK_CONFIG_H
#define __NETWORK_CONFIG_H

/*===========================================================================
 * WiFi STA (Station Mode) Credentials
 *===========================================================================*/
#define WIFI_SSID           "USIDE-2F"
#define WIFI_PASSWORD       "uside8888"
#define WIFI_SECURITY       "wpa2_aes"

/*===========================================================================
 * Network Mode Selection
 *   Uncomment exactly ONE of the following:
 *===========================================================================*/
#define NETWORK_MODE_STA        // STA mode — connect to existing AP (DHCP client)
// #define NETWORK_MODE_AP       // AP mode — module acts as access point

/*===========================================================================
 * Remote Server (HLK-RM58S transparent TCP target)
 *===========================================================================*/
#define REMOTE_IP           "192.168.31.144"
#define REMOTE_PORT         "9998"
#define PROTOCOL            "tcp"

/*===========================================================================
 * MQTT Broker (via LwIP Ethernet stack)
 *===========================================================================*/
#define MQTT_BROKER_ADDR    "192.168.31.144"
#define MQTT_BROKER_PORT    1883
#define MQTT_CLIENT_ID      "STM32_Client_001"
#define MQTT_USERNAME       "STM32"
#define MQTT_PASSWORD       "STM32"

/*===========================================================================
 * AP Mode Network Parameters (used when NETWORK_MODE_AP is enabled)
 *===========================================================================*/
#define AP_IP_BASE          "192.168.31"
#define AP_GATEWAY          "192.168.31.254"
#define AP_NETMASK          "255.255.255.0"

/*===========================================================================
 * DHCP Server Config (AP mode)
 *===========================================================================*/
#define DHCP_START_IP       "192.168.31.1"
#define DHCP_END_IP         "192.168.31.200"
#define DHCP_DNS1           "192.168.31.254"
#define DHCP_DNS2           "8.8.8.8"
#define DHCP_LEASE_TIME     "86400"

/*===========================================================================
 * UART Serial Port (HLK-RM58S communication)
 *===========================================================================*/
#define UART_BAUDRATE       "115200"
#define UART_DATABITS       "8"
#define UART_PARITY         "n"
#define UART_STOPBITS       "1"
#define UART_PACK_LEN       "64"
#define UART_PACK_TIMEOUT   "10"

/*===========================================================================
 * Timeout Settings
 *===========================================================================*/
#define RESPONSE_TIMEOUT    2000
#define CONNECTION_TIMEOUT  "0"

/*===========================================================================
 * WOL (Wake-on-LAN) Target MAC Address
 *===========================================================================*/
#define WOL_TARGET_MAC_STR      "AA:BB:CC:DD:EE:FF"          // 字符串格式，便于打印调试
#define WOL_TARGET_MAC_BYTE     {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}  // 字节数组，用于构造魔术包

/*===========================================================================
 * WOL UDP Port (标准端口: 7 或 9)
 *===========================================================================*/
#define WOL_UDP_PORT            9

/*===========================================================================
 * WOL Broadcast Address
 *===========================================================================*/
#define WOL_BROADCAST_IP        "255.255.255.255"

#endif /* __NETWORK_CONFIG_H */
