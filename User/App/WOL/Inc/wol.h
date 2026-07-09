#ifndef __WOL_H
#define __WOL_H

#include "user_network.h"
#include "lwip/err.h"

/**
  * @brief  构造并发送WOL魔术包（使用预定义的targetMac）
  * @retval err_t: ERR_OK表示成功，其他值表示失败
  */
err_t send_wol_magic_packet(void);

#endif