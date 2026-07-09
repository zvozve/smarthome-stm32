#ifndef __HLK_RM58S_APP_H__
#define __HLK_RM58S_APP_H__

#include "hlk_rm58s_driver.h"
#include "hlk_rm58s_func.h"


void hlk_rm58s_task(void);

void hlk_rm58s_init(void);

void mbMaster_Test(void);
void wifiMaster_Poll(Uart_Task_t *task);
void wifiMaster_Callback(void);
void USR_UART_Print_STA(const char *sFormat, uart_Status_e mbtStatus, wifiStatus_t Status);

#endif // __HLK_RM58S_APP_H__
