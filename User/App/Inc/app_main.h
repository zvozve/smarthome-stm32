#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include "cmsis_os2.h"
#include "SEGGER_RTT.h"

#define FIRMWARE_VERSION  "V1.0.1/2026-07-15"

extern osThreadId_t appTaskHandle;
extern osThreadId_t wifiTaskHandle;

void app_main_init(void);
void app_main_create_tasks(void);

#endif