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

/* PHY Hardware Reset Pin — change to match your schematic */
#define ETH_RST_GPIO_Port     GPIOD
#define ETH_RST_Pin           GPIO_PIN_3

void ETH_PHY_HardwareReset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ETH_PHY_H */
