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

/* ---- 心跳 LED（CPU_STA, PC13）+ 看门狗（本工程未启用 IWDG） ---- */
#define BOARD_HEART_LED_PORT    CPU_STA_GPIO_Port
#define BOARD_HEART_LED_PIN     CPU_STA_Pin
#define BOARD_HEART_IWDG        NULL          /* app 仍传 NULL（运行时安全空操作） */
#define BOARD_HEART_IWDG_ENABLE 0             /* 经 MB_BOARD_CFG 由 SDK heart_beat 读取：0=不引 oop_iwdg_drv.h、不喂狗 */

/* ---- 红外接收（VS1838B_DAT, PE5 = TIM9_CH1, AF3）----
 * 新 SDK ir_receiver 用 TIM 输入捕获；计数频率 = TIM_CLK/(PSC+1)。
 * 本工程把接收 TIM 的 PSC 配成 167（TIM9 168MHz -> 1MHz 计数），ARR 取满量程，
 * 以获得足够长的静默/帧结束检测窗口（NEC 引导码 9ms 不应被截断）。
 * 后续若按 CubeMX 把收/发分到不同定时器，只改下面 BOARD_IR_RX_TIM 宏即可。
 */
#define BOARD_IR_RX_TIM              (&htim9)
#define BOARD_IR_RX_CHANNEL          TIM_CHANNEL_1
#define BOARD_IR_RX_CLK_HZ           1000000UL   /* TIM9 PSC=167 -> 1MHz 计数 */
#define BOARD_IR_RX_BUF_SIZE         256         /* 时长缓冲条目数（偶数=mark/奇数=space） */
#define BOARD_IR_RX_IDLE_US          0           /* 0 = 驱动默认 15000us */
#define BOARD_IR_RX_DEBOUNCE_US      0           /* 0 = 驱动默认 200us */

/* ---- 红外发射（PE6 = TIM9_CH2, AF3；38.46kHz 载波，占空比 ~35%）----
 * 新 SDK ir_transmitter 自配 PWM 载波 + 注入的 µs 时基定时器（tick）。
 * 当前收/发共用 TIM9（硬件 PE5/PE6 均 AF3-TIM9），为临时态；发送会在 Task_IR_SendAc
 * 内临时把 TIM9 配成 PWM 载波、发完重配回接收参数。后续 CubeMX 把发射改到独立定时器
 * 后，只改 BOARD_IR_TX_TIM / BOARD_IR_TICK_TIM 宏，ARR 冲突即消失，无需再手工恢复。
 * TIM9 时钟由 board_io_init() 开启；GPIO AF 由本文件 MspInit 段开启。
 */
#define BOARD_IR_TX_TIM              (&htim9)
#define BOARD_IR_TX_CHANNEL          TIM_CHANNEL_2
#define BOARD_IR_TX_TIM_CLK_HZ       168000000UL /* TIM9 挂 APB2 = 168MHz */
#define BOARD_IR_TX_CARRIER_HZ       38461UL     /* 38.46kHz */
#define BOARD_IR_TX_DUTY             35
#define BOARD_IR_TX_INVERTED         0
#define BOARD_IR_TX_USE_CARRIER      1
#define BOARD_IR_TICK_TIM            (&htim6)    /* µs 时基（基本定时器） */
#define BOARD_IR_TICK_CLK_HZ         84000000UL  /* TIM6 挂 APB1 = 84MHz */

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
    __HAL_RCC_TIM9_CLK_ENABLE();   /* 接收输入捕获（MspInit 亦开，这里先开稳妥） */
    __HAL_RCC_TIM3_CLK_ENABLE();   /* 发射 PWM 载波 */
    MX_TIM9_Init();                /* TIM9 实例/PSC/ARR（1MHz 计数，长静默窗口） */
    MX_TIM3_Init();                /* TIM3 实例（PWM/时基由驱动自配） */
}

#ifdef __cplusplus
}
#endif

#endif /* __BOARD_CFG_H */
