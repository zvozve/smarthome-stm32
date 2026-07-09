#include "app_entry.h"
#include "lan8720a_port.h"
#include "wol.h"
#include "hlk_rm58s_app.h"

/* Definitions for app_task */
osThreadId_t userAppDefaultTaskHandle;
const osThreadAttr_t userAppDefaultTask_attributes = {
  .name = "userAppDefaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

void userAppDefaultTask(void *argument);

void userApp_Init(void)
{
  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "[APP] System initializing...\n");

  ETH_PHY_HardwareReset();
  SEGGER_RTT_printf(0, "[APP] ETH_PHY_HardwareReseted...\n");

  hlk_rm58s_init();
  hlk_rm58s_task();
}

void userApp_CreateTask(void) 
{
  userAppDefaultTaskHandle = osThreadNew(userAppDefaultTask, NULL, &userAppDefaultTask_attributes);
}

void userAppDefaultTask(void *argument)
{
  osDelay(2000);

  SEGGER_RTT_printf(0, "[APP] Task started!\n");

  while (1) {
      err_t result = send_wol();
      
      if (result == ERR_OK) {
          APP_LOG("WOL sent successfully via LAN\r\n");
          APP_LOG("WOL task idle, waiting...\r\n");
          // 成功则永久等待（任务挂起），不再发送
          osThreadSuspend(NULL);  // 挂起当前任务
          while(1) { osDelay(1000); }
      } else {
          APP_LOG("WOL failed (err=%d), retrying in 2s...\r\n", result);
          osDelay(2000);  // 失败则2秒后重试
      }
  }
}
