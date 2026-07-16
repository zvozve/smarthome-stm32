#ifndef __TASK_UART_TEST_H
#define __TASK_UART_TEST_H

#include <stdbool.h>
#include <stdint.h>

void uart_test_init(void);
bool uart_test_send(const char *cmd, char *response, uint16_t len);

#endif