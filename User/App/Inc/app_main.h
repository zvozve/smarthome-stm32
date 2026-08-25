#ifndef __APP_MAIN_H
#define __APP_MAIN_H

#include "main.h"
#include "cmsis_os2.h"

#ifndef APP_LOG_ENABLE
    #define APP_LOG_ENABLE     1
#endif
#include "SEGGER_RTT_Log.h"
#define APP_LOG(fmt, ...)     RTT_LOG_TAG(APP_LOG_ENABLE,    "APP",    fmt, ##__VA_ARGS__)

#define FIRMWARE_VERSION  "V1.0.1/2026-07-15"

void app_main_init(void);
void app_main_create_tasks(void);
void app_main_task(void);

#endif