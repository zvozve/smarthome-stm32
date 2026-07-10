/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : eth_phy.h
  * @brief          : Ethernet PHY hardware reset
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __ETH_PHY_H
#define __ETH_PHY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

// ===========================
// 调试宏
// ===========================

#define LAN_DEBUG_ENABLE    1

#if LAN_DEBUG_ENABLE
    #include "SEGGER_RTT.h"
    #define LAN_LOG(fmt, ...)   SEGGER_RTT_printf(0, "[LAN] " fmt, ##__VA_ARGS__)
#else
    #define LAN_LOG(fmt, ...)   ((void)0)
#endif

/* PHY Hardware Reset Pin — change to match your schematic */
#define ETH_RST_GPIO_Port     GPIOD
#define ETH_RST_Pin           GPIO_PIN_3

void LAN_HardwareReset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ETH_PHY_H */
