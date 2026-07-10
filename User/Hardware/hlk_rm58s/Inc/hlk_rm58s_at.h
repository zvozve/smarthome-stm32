#ifndef __HLK_RM58S_AT_H
#define __HLK_RM58S_AT_H

// ===========================
// AT命令定义（仅供参考，驱动内部使用）
// ===========================

#define AT_VER              "at+ver=?"
#define AT_NETMODE_QUERY    "at+netmode=?"
#define AT_WIFI_CONF_QUERY  "at+wifi_conf=?"
#define AT_WIFI_CONN_QUERY  "at+wifi_ConState=?"

#define AT_NETMODE_STA      "at+netmode=2"
#define AT_NETMODE_AP       "at+netmode=3"

#define AT_DHCPC_ON         "at+dhcpc=1"
#define AT_DHCPC_OFF        "at+dhcpc=0"

#define AT_REMOTEIP         "at+remoteip=%s"
#define AT_REMOTEPORT       "at+remoteport=%d"
#define AT_REMOTEPRO        "at+remotepro=%s"

#define AT_MODE_CLIENT      "at+mode=client"
#define AT_MODE_SERVER      "at+mode=server"

#define AT_NET_COMMIT       "at+net_commit=1"
#define AT_RECONN           "at+reconn=1"
#define AT_DEFAULT          "at+default=1"
#define AT_REBOOT           "at+reboot=1"

#endif /* __HLK_RM58S_AT_H */