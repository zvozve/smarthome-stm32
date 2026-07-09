#include "wol.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

// 使用user_network.h中定义的MAC地址
const uint8_t targetMac[6] = WOL_TARGET_MAC_BYTE;

// // 在main.c或其他地方调用
// if (send_wol_magic_packet() == ERR_OK) {
//     printf("WOL packet sent successfully!\r\n");
// } else {
//     printf("Failed to send WOL packet!\r\n");
// }

// // 或者连续发送多次提高成功率
// for (int i = 0; i < 3; i++) {
//     if (send_wol_magic_packet() == ERR_OK) {
//         printf("WOL packet sent (attempt %d)\r\n", i + 1);
//         break;
//     }
//     HAL_Delay(100);
// }

/**
  * @brief  构造并发送WOL魔术包
  * @retval err_t: ERR_OK表示成功，其他值表示失败
  */
err_t send_wol_magic_packet(void) {
    struct udp_pcb *udp_pcb;
    struct pbuf *pkt_buf;
    err_t err;
    ip_addr_t dest_ip;

    // 魔术包结构: 6字节同步码(0xFF) + 16次目标MAC地址
    uint8_t magic_packet[102] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    for (int i = 6, j = 0; i < 102; i++, j = (j + 1) % 6) {
        magic_packet[i] = targetMac[j];
    }

    // 1. 申请内存包
    pkt_buf = pbuf_alloc(PBUF_TRANSPORT, sizeof(magic_packet), PBUF_ROM);
    if (pkt_buf == NULL) {
        return ERR_MEM;
    }
    pkt_buf->payload = magic_packet;
    pkt_buf->len = pkt_buf->tot_len = sizeof(magic_packet);

    // 2. 创建UDP控制块 (PCB)
    udp_pcb = udp_new();
    if (udp_pcb == NULL) {
        pbuf_free(pkt_buf);
        return ERR_MEM;
    }

    // 3. 绑定到本地端口 (使用WOL_UDP_PORT)
    err = udp_bind(udp_pcb, IP_ADDR_ANY, WOL_UDP_PORT);
    if (err != ERR_OK) {
        pbuf_free(pkt_buf);
        udp_remove(udp_pcb);
        return err;
    }

    // 4. 设置目标IP为广播地址
    IP4_ADDR(&dest_ip, 255, 255, 255, 255);
    // 或者用字符串方式：
    // inet_aton(WOL_BROADCAST_IP, &dest_ip);
    
    // 5. 连接并发送数据
    err = udp_connect(udp_pcb, &dest_ip, WOL_UDP_PORT);
    if (err != ERR_OK) {
        pbuf_free(pkt_buf);
        udp_remove(udp_pcb);
        return err;
    }
    
    err = udp_send(udp_pcb, pkt_buf);
    
    // 6. 断开连接并释放资源
    udp_disconnect(udp_pcb);
    pbuf_free(pkt_buf);
    udp_remove(udp_pcb);
    
    return err;
}