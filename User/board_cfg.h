#ifndef __BOARD_CFG_H
#define __BOARD_CFG_H

/*
 * board_cfg.h —— 工程侧硬件绑定（类 devicetree，模板由 sync_lib.py 生成）
 *
 * 规则：
 *   - 全工程（User/ 下）唯一允许 include CubeMX 生成头
 *     （main.h / usart.h / tim.h ...）的地方就是本文件；
 *     SDK（MySDK/）不做任何绑定。
 *   - app / tasks 只引用本文件的绑定宏，不直接引用 MX 符号。
 *   - 换板只改本文件（引脚、句柄、时钟），SDK 与业务代码不动。
 *
 * 板卡：SmartHome (STM32F407)，引脚/句柄取自 Core/Inc/main.h|tim.h|usart.h
 */

#include "main.h"
#include "usart.h"     /* huart6 */
#include "tim.h"       /* htim6 */

#ifdef __cplusplus
extern "C" {
#endif

/* ========== 绑定区 ========== */

/* ---- 心跳 LED（CPU_STA, PC13）+ 看门狗（本工程未启用 IWDG → NULL） ---- */
#define BOARD_HEART_LED_PORT    CPU_STA_GPIO_Port
#define BOARD_HEART_LED_PIN     CPU_STA_Pin
#define BOARD_HEART_IWDG        NULL

/* ---- 红外接收 1838B（VS1838B_DAT, PE5）+ 1ms 节拍 TIM6 ---- */
#define BOARD_IR_RX_PORT        VS1838B_DAT_GPIO_Port
#define BOARD_IR_RX_PIN         VS1838B_DAT_Pin
#define BOARD_IR_RX_TIM         (&htim6)

/* ---- 红外发射（PE6 = TIM9_CH2, AF3；38.46kHz 载波，占空比 ~35%） ----
 * TIM9 未入 CubeMX（.ioc），时基/通道由 ir_tx 驱动按此配置初始化，
 * 时钟（GPIOE + TIM9）由下面 board_io_init() 开启。
 * TIM9 挂 APB2，定时器时钟 168MHz：PSC=167 → 1MHz；ARR=25 → 26us ≈ 38.46kHz。
 */
#define BOARD_IR_TX_CFG         { GPIOE, GPIO_PIN_6, GPIO_AF3_TIM9, \
                                  TIM9, TIM_CHANNEL_2, 167, 25, 9 }

/* ---- DHT11 温湿度（DHT11_DAT, PE2） ---- */
#define BOARD_DHT11_PORT        DHT11_DAT_GPIO_Port
#define BOARD_DHT11_PIN         DHT11_DAT_Pin

/* ---- ETH PHY LAN8720A 复位（ETH_RST, PD3） ---- */
#define BOARD_ETH_RST_PORT      ETH_RST_GPIO_Port
#define BOARD_ETH_RST_PIN       ETH_RST_Pin

/* ---- WiFi 模组 HLK-RM58S：UART6 115200 + 控制引脚 ---- */
#define BOARD_HLK_UART          (&huart6)
#define BOARD_HLK_ES_PORT       WIFI_ES_GPIO_Port
#define BOARD_HLK_ES_PIN        WIFI_ES_Pin
#define BOARD_HLK_RST_PORT      WIFI_RST_GPIO_Port
#define BOARD_HLK_RST_PIN       WIFI_RST_Pin
#define BOARD_HLK_STS_PORT      WIFI_STS_GPIO_Port
#define BOARD_HLK_STS_PIN       WIFI_STS_Pin
#define BOARD_HLK_SOT_PORT      WIFI_SOT_GPIO_Port
#define BOARD_HLK_SOT_PIN       WIFI_SOT_Pin

/* ---- Wake-on-LAN：目标 PC MAC + LAN 网口（来自 LwIP） ----
 * gnetif 由 LWIP/App/lwip.c 定义并初始化，这里只取地址；
 * 目标 MAC 取自 config_network.h 的 TARGET_PC_MAC_BYTE。
 */
#include "config_network.h"   /* TARGET_PC_MAC_BYTE */
#define BOARD_WOL_TARGET_MAC   ((const uint8_t[6])TARGET_PC_MAC_BYTE)
extern struct netif gnetif;   /* LwIP 全局网口（LWIP/App/lwip.c） */
#define BOARD_WOL_NETIF        (&gnetif)

/* ---------- 非 CubeMX 管理的外设时钟 ----------
 * SDK 驱动不接管 RCC（HAL 泄漏红线），未入 .ioc 的外设时钟在此开启。
 * GPIOE 时钟已由 CubeMX 开启（PE2/PE5 在用）；TIM9 需补开。
 */
static inline void board_io_init(void)
{
    __HAL_RCC_TIM9_CLK_ENABLE();
}

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CFG_H */
