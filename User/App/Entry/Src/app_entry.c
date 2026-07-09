#include "app_entry.h"
#include "SEGGER_RTT.h"
#include "wol.h"
#include "hlk_rm58s_app.h"

extern ETH_HandleTypeDef heth;

/* Definitions for app_task */
osThreadId_t appTaskHandle;
const osThreadAttr_t appTask_attributes = {
  .name = "appTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/**
  * @brief  App initialization, called before kernel start.
  * @note   Place pre-scheduler initializations here.
  */
void App_Init(void)
{
  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "[APP] System initializing...\n");
  hlk_rm58s_init();
  hlk_rm58s_task();
}

void App_CreateTask(void) 
{
  appTaskHandle = osThreadNew(App_TaskStart, NULL, &appTask_attributes);
}

/**
  * @brief  App task entry, runs after scheduler starts.
  * @param  argument : Not used
  */
void App_TaskStart(void *argument)
{
  osDelay(2000);

  SEGGER_RTT_printf(0, "[APP] Task started!\n");

  while (1) {
      err_t result = send_wol();
      
      if (result == ERR_OK) {
          SEGGER_RTT_printf(0, "[APP] WOL sent successfully via LAN\r\n");
          SEGGER_RTT_printf(0, "[APP] WOL task idle, waiting...\r\n");
          // 成功则永久等待（任务挂起），不再发送
          osThreadSuspend(NULL);  // 挂起当前任务
          while(1) { osDelay(1000); }
      } else {
          SEGGER_RTT_printf(0, "[APP] WOL failed (err=%d), retrying in 2s...\r\n", result);
          osDelay(2000);  // 失败则2秒后重试
      }
  }
}
