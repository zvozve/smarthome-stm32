/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : user_app.h
  * @brief          : User application header
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __USER_APP_H
#define __USER_APP_H

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#define APP_DEBUG_ENABLE    1

#if APP_DEBUG_ENABLE
    #include "SEGGER_RTT.h"
    #define APP_LOG(fmt, ...)   SEGGER_RTT_printf(0, "[APP] " fmt, ##__VA_ARGS__)
#else
    #define APP_LOG(fmt, ...)   ((void)0)
#endif

void userApp_Init(void);
void userApp_CreateTask(void);

#endif /* __USER_APP_H */
