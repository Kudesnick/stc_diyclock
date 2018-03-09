#ifndef _UART_H_
#define _UART_H_

#include "stc15.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STATUS_IDLE,
    STATUS_TX,
    STATUS_RX,
} uart_status_t;

void uart_init(void);
uart_status_t uart_get_status(void);
bool uart_send(const uint8_t * str);
void uart1_isr() __interrupt 4 __using 2;

#endif /* _UART_H_ */
