#ifndef __HLK_RM58S_DRIVER_H__
#define __HLK_RM58S_DRIVER_H__

#include "hlk_rm58s_port.h"
#include "network_config.h"

#define RESPONSE_TIMEOUT    2000

/*===========================================================================
 * UART Serial Port (HLK-RM58S communication)
 *===========================================================================*/
#define UART_BAUDRATE       "115200"
#define UART_DATABITS       "8"
#define UART_PARITY         "n"
#define UART_STOPBITS       "1"
#define UART_PACK_LEN       "64"
#define UART_PACK_TIMEOUT   "10"

// 超时
#define CONNECTION_TIMEOUT "0"

// ===========================
// AT 指令定义 (对照HLK-RM58S规格书)
// ===========================
// 格式: at+[command]=[value],[value],...\r\n
// 查询: at+[command]=?

// 查询指令
#define AT_VER             "at+ver=?\r\n"
#define AT_NETMODE_QUERY   "at+netmode=?\r\n"
#define AT_WIFI_CONF_QUERY "at+wifi_conf=?\r\n"
#define AT_UART_QUERY      "at+uart=?\r\n"
#define AT_WIFI_CONN_QUERY "at+wifi_ConState=?\r\n"  // 查询STA连接状态

// 网络模式 (1=配网, 2=STA, 3=2.4G_AP, 4=5.8G_AP)
#define AT_NETMODE_STA     "at+netmode=2\r\n"
#define AT_NETMODE_AP      "at+netmode=3\r\n"

// WiFi配置 (格式: at+wifi_conf=SSID,加密方式,密码)
// 加密方式: none/wep/wpa_psk/wpa2_psk/wpa_wpa2_psk
#define AT_WIFI_CONF       "at+wifi_conf=" WIFI_SSID "," WIFI_SECURITY "," WIFI_PASSWORD "\r\n"

// DHCP (1=DHCP动态IP, 0=静态IP)
#define AT_DHCPC_ON        "at+dhcpc=1\r\n"
#define AT_DHCPC_OFF       "at+dhcpc=0\r\n"

// AP模式DHCP服务
#define AT_DHCPD_ON        "at+dhcpd=1\r\n"
#define AT_DHCPD_OFF       "at+dhcpd=0\r\n"
#define AT_DHCPD_IP        "at+dhcpd_ip=" DHCP_START_IP "," DHCP_END_IP "," AP_NETMASK "," AP_GATEWAY "\r\n"
#define AT_DHCPD_DNS       "at+dhcpd_dns=" DHCP_DNS1 "," DHCP_DNS2 "\r\n"
#define AT_DHCPD_TIME      "at+dhcpd_time=" DHCP_LEASE_TIME "\r\n"

// 网络参数 (格式: at+net_ip=IP,掩码,网关)
#define AT_NET_IP          "at+net_ip=" AP_GATEWAY "," AP_NETMASK "," AP_GATEWAY "\r\n"
#define AT_NET_DNS         "at+net_dns=" DHCP_DNS1 "," DHCP_DNS2 "\r\n"

// 远程服务器
#define AT_REMOTEIP        "at+remoteip=" REMOTE_IP "\r\n"
#define AT_REMOTEPORT      "at+remoteport=" REMOTE_PORT "\r\n"
#define AT_REMOTEPRO       "at+remotepro=" PROTOCOL "\r\n"

// 其他
#define AT_TIMEOUT         "at+timeout=" CONNECTION_TIMEOUT "\r\n"
#define AT_MODE_SERVER     "at+mode=server\r\n"
#define AT_MODE_CLIENT     "at+mode=client\r\n"
#define AT_UART            "at+uart=" UART_BAUDRATE "," UART_DATABITS "," UART_PARITY "," UART_STOPBITS "\r\n"
#define AT_UARTPACKLEN     "at+uartpacklen=" UART_PACK_LEN "\r\n"
#define AT_UARTPACKTIMEOUT "at+uartpacktimeout=" UART_PACK_TIMEOUT "\r\n"
#define AT_NET_COMMIT      "at+net_commit=1\r\n"
#define AT_RECONN          "at+reconn=1\r\n"
#define AT_DEFAULT         "at+default=1\r\n"
#define AT_REBOOT          "at+reboot=1\r\n"


// ===========================
// WiFi 配置序列 (STA模式，对照规格书)
// ===========================
// 已在 hlk_rm58s_driver.c 的 hlk_rm58s_configure_WiFi_Module() 中实现
// STA模式配置步骤:
//  1. at+netmode=2           (STA模式)
//  2. at+wifi_conf=SSID,加密,密码
//  3. at+dhcpc=1             (DHCP开启)
//  4. at+remoteip=IP         (远端IP)
//  5. at+remoteport=PORT    (远端端口)
//  6. at+remotepro=tcp      (协议)
//  7. at+mode=client         (客户端模式)
//  8. at+uart=...            (串口参数)
//  9. at+net_commit=1       (提交)
// 10. at+reconn=1           (重启)

typedef enum { WIFI_MODE_STA, WIFI_MODE_AP } WiFiMode_t;

typedef enum {
    MB_NO_ERR               = 0x00,     
    MB_CMD_FXCODE_ERR       = 0x01, // 功能码错误
    MB_CMD_ADDR_ERR         = 0x02, // 域数据首地址错误
    MB_DATA_ERR             = 0x03, // 数据内容错误
    MB_CMD_ID_ERR           = 0x04, 
    MB_CMD_LEN_ERR          = 0x05, 
    MB_CMD_CRC_ERR          = 0x06, 
    MB_DATA_TYPE_ERR        = 0x10, // 数据类型错误
    MB_DATA_LEN_ERR         = 0x11, // 域数据个数错误
    MB_DATA_LEN_NO_MATCH    = 0x12, // 数据个数与内部区域数据个数不符
    MB_DATA_CANT_WRITE      = 0x13, // 不能进行写操作    
} wifiStatus_t;

typedef struct {
    uint8_t  id;       
	uint8_t  fxcode;              
    uint16_t addr;
	uint16_t dataCnt;
	uint8_t  dataByte;
    bool     *rdBit;
    uint16_t *rdU16;
    bool     wtBit[UART_BUF_LEN];
    uint16_t wtU16[UART_BUF_LEN];
    uint8_t  cmdBuf[UART_BUF_LEN];
    uint16_t cmdByte; 
}wifiCommand_t;

typedef struct {
    uint8_t         ID;
    wifiCommand_t   Tx;
    wifiCommand_t   Rx;
    wifiStatus_t    Status;
}wifi_t;

extern wifi_t wifiMaster;

extern char jsonTxBuf[UART_BUF_LEN];
extern char jsonRxBuf[UART_BUF_LEN];
extern bool rx_flag;

// ===========================
// 函数声明
// ===========================
void hlk_rm58s_gpio_init(void);
void hlk_rm58s_gpio_active_at_mode(void);
void hlk_rm58s_gpio_avtive_reset(void);
void hlk_rm58s_gpio_active_recovery_mode(void);

char *strcasestr_local(const char *haystack, const char *needle);
void build_json_message(char* buf, size_t buf_size);
void parse_json(char *json);

void wifiMaster_cmd_clr(void);
void wifiMaster_transfer_block(uint16_t ms);
void wifiMaster_transfer_at_cmd(const char *command, const char *expected_response, uint32_t timeout);
int wifiMaster_ParseResponse(const char *response, const char *expected);
int wifiMaster_WaitResponse(const char *expected, uint32_t timeout_ms);

void hlk_rm58s_configure_WiFi_Module(void);

#endif // __HLK_RM58S_DRIVER_H__
