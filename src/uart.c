#include "stc15.h"
#include <stdbool.h>
#include <string.h>

#include "hwconfig.h"
#include "uart.h"

#define BAUDRATE 9600

#define BUF_SIZE 16

#define UART_SW1_1 (0 << 6) // P3.0 - Rx, P3.1 - Tx (default)
#define UART_SW1_2 (1 << 6) // P3.6 - Rx, P3.7 - Tx
#define UART_SW1_3 (2 << 6) // P1.6 - Rx, P1.7 - Tx

uint8_t uart_buf[BUF_SIZE+1];

volatile uint8_t data_ptr;
uart_status_t uart_status;

void uart_init(void)
{
//    P_SW1 |= UART_SW1_2; // move UART1 pins -> P3_6:rxd, P3_7:txd

    // UART1 use Timer2
    T2L = (uint8_t)((0x10000 - (FOSC/4/BAUDRATE)) & 0xFF); // 0xE0
    T2H = (uint8_t)((0x10000 - (FOSC/4/BAUDRATE)) >> 8);    // 0xFE

    SM1 = 1;        // serial mode 1: 8-bit async
    AUXR |= 0x14;   // T2R: run T2, T2x12: T2 clk src sysclk/1
    AUXR |= 0x01;   // S1ST2: T2 is baudrate generator

    TI = 0;
    RI = 0;
    ES = 1;         // enable uart1 interrupt
    EA = 1;         // enable interrupts
    REN = 1;        // enable rx
}

uart_status_t uart_get_status(void)
{
    return uart_status;
}

bool uart_send(const uint8_t * str)
{
    uint8_t size;

    ES = 0;

    if (   TI
        || uart_status != STATUS_IDLE
        || str == NULL
        || str[0] == '\0'
        )
    {
        ES = 1;
        return false;
    }

    REN = 0;

    uart_status = STATUS_TX;
    size = strlen(str);
    if (size > BUF_SIZE)
    {
        size = BUF_SIZE;
    }
    memcpy(uart_buf, str, size);
    uart_buf[size] = '\0';
    data_ptr = 0;

    TI = 0;
    SBUF = uart_buf[data_ptr];

    ES = 1;
    return true;
}

uint8_t * uart_get(void)
{
    if (uart_status != STATUS_RX_COMPLETE)
    {
        return NULL;
    }
    else
    {
        uart_status = STATUS_IDLE;
        RI = 0;
        REN = 1;

        return  uart_buf;
    }
}

void uart1_isr(void) __interrupt 4 __using 2
{
    ES = 0;

    if (RI)
    {
        uint8_t ch = SBUF;

        RI = 0; //clear rx int

        switch (uart_status)
        {
            case STATUS_IDLE:
            {
                if (ch != '\r' && ch != '\n' && ch != '\0')
                {
                    data_ptr = 1;
                    uart_buf[0] = ch;
                    uart_status == STATUS_RX;
                }
                break;
            }
            case STATUS_RX:
            {
                if (ch != '\r' && ch != '\n' && ch != '\0' && data_ptr < BUF_SIZE)
                {
                    uart_buf[data_ptr++] = ch;
                }
                else
                {
                    uart_buf[data_ptr] = '\0';
                    uart_status = STATUS_RX_COMPLETE;

                    REN = 0;
                }
            }
            default:
            {
                REN = 0;
                break;
            }
        }
    }
    if (TI)
    {
        TI = 0;

        if (uart_status == STATUS_TX)
        {
            data_ptr++;

            if (uart_buf[data_ptr] != '\0' && data_ptr < BUF_SIZE)
            {
                SBUF = uart_buf[data_ptr];
            }
            else
            {
                uart_status = STATUS_IDLE;

                REN = 1;
            }
        }
    }

    ES = 1;
}
