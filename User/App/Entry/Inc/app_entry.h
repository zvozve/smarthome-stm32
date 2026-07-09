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

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

void App_Init(void);
void App_CreateTask(void);
void App_TaskStart(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __USER_APP_H */
