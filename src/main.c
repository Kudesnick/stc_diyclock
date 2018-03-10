//
// STC15F204EA DIY LED Clock
// Copyright 2016, Jens Jensen
//

#include "stc15.h"
#include <stdint.h>
#include <stdio.h>
#include "adc.h"
#include "ds1302.h"
#include "led.h"
#include "uart.h"
#include "hwconfig.h"
#include "keybrd.h"

// clear wdt
#define WDT_CLEAR()    (WDT_CONTR |= 1 << 4)

// display mode states
typedef enum {
    M_DEMO,
    M_NORMAL,
    M_SEC_DISP,
    M_TIMER,
} display_mode_t;

// display mode states
typedef enum keybrd_mode {
    K_DEMO,
    K_NORMAL,
    K_SET_HOUR,
    K_SET_MINUTE,
    K_SEC_DISP,
    K_TIMER,
} keybrd_mode_t;

/* ------------------------------------------------------------------------- */

void _delay_ms(uint8_t ms)
{
    // delay function, tuned for 11.092 MHz clock
    // optimized to assembler
    ms; // keep compiler from complaining?
    __asm;
        ; dpl contains ms param value
    delay$:
        mov	b, #8   ; i
    outer$:
        mov	a, #243    ; j
    inner$:
        djnz acc, inner$
        djnz b, outer$
        djnz dpl, delay$
    __endasm;
}

// Buzzer
volatile __bit   buzzer_set_on = 1;
volatile __bit   buzzer_must_on;
uint8_t          buzzer_bufer_hh;
volatile uint8_t buzzer_counter;

// Display brightness
volatile uint8_t disp_bright_cnt;
uint8_t          disp_bright_val;  // light sensor value

volatile __bit blinker_slow;
volatile __bit blinker_fast;
volatile __bit loop_gate;

uint8_t dmode = M_DEMO;
uint8_t kmode = K_DEMO;

__bit  blink_seg_01;
__bit  blink_seg_23;

uint8_t rtc_hh_bcd;
uint8_t rtc_mm_bcd;

uint8_t        tmr_mm;
uint8_t        tmr_ss;
uint8_t        tmr_ms_x10;
volatile __bit tmr_is_runing;

void tmr_inc(void)
{
    if (tmr_is_runing)
    {
        if (++tmr_ms_x10 > 99)
        {
            tmr_ms_x10 = 0;
            if (++tmr_ss > 59)
            {
                tmr_ss = 0;
                if (++tmr_mm > 99) tmr_mm = 0;
            }
        }
    }
}

void timer0_isr() __interrupt 1 __using 1
{
    volatile static int8_t  count_100;
    volatile static int8_t count_1000;
    volatile static int8_t count_5000;

    // display refresh ISR
    // cycle thru digits one at a time
    uint8_t digit = disp_bright_cnt % 4;

    // turn off all digits, set high
    LED_DIGITS_OFF();

    // auto dimming, skip lighting for some cycles
    if (disp_bright_cnt++ % disp_bright_val < 4 ) {
        // fill digits
        LED_SEGMENT_PORT = dbuf[digit];
        // turn on selected digit, set low
        LED_DIGIT_ON(digit);
    }

    if (buzzer_must_on)
    {
        if (buzzer_counter++ == 16)
        {
            BUZZER_OFF;
            buzzer_must_on = 0;
            buzzer_counter = 0;
        }
        else if (buzzer_counter % 8 == 0)
        {
            BUZZER_ON;
        }
        else
        {
            BUZZER_OFF;
        }
    }

    // 100/sec: 10 ms
    if (count_100++ == 100)
    {
        count_100 = 0;
        // 10/sec: 100 ms
        if (count_1000++ == 10)
        {
            count_1000 = 0;
            blinker_fast = !blinker_fast;
            loop_gate = 1;
            // 2/sec: 500 ms
            if (count_5000++ == 5)
            {
                count_5000 = 0;
                blinker_slow = !blinker_slow;
            }
        }

        keybrd_routine();
        tmr_inc();
    }
}


// Call timer0_isr() 10000/sec: 0.0001 sec
// Initialize the timer count so that it overflows after 0.0001 sec
// THTL = 0x10000 - FOSC / 12 / 10000 = 0x10000 - 92.16 = 65444 = 0xFFA4
void Timer0Init(void)		//100us @ 11.0592MHz
{
    // refer to section 7 of datasheet: STC15F2K60S2-en2.pdf
    // TMOD = 0;    // default: 16-bit auto-reload
    // AUXR = 0;    // default: traditional 8051 timer frequency of FOSC / 12
    // Initial values of TL0 and TH0 are stored in hidden reload registers: RL_TL0 and RL_TH0
	TL0 = 0xA4;		// Initial timer value
	TH0 = 0xFF;		// Initial timer value
    TF0 = 0;		// Clear overflow flag
    TR0 = 1;		// Timer0 start run
    ET0 = 1;        // Enable timer0 interrupt
    EA = 1;         // Enable global interrupt
}

void colon_blink(__bit blinker)
{
    if (blinker)
    {
        dotdisplay(1, 1);
        dotdisplay(2, 1);
    }
}

/*********************************************/
int main()
{
    uint8_t i;
    // 1 seconds delay
    for (i = 10; i > 0; i--) _delay_ms(100);

#ifndef WITHOUT_LIGHT
    // SETUP
    // set photoresistor & ntc pins to open-drain output
    P1M1 |= (1<<ADC_LIGHT);
    P1M0 |= (1<<ADC_LIGHT);
#endif

    // init rtc
    ds_init();
    // init/read ram config
    ds_ram_config_init();

    uart_init();

    //ds_reset_clock(); // uncomment in order to reset minutes and hours to zero.. Should not need this.

    Timer0Init(); // display refresh & switch read

    // LOOP
    while (1)
    {
        static uint8_t loop_count;

        keybrd_event_t ev;

        while (!loop_gate); // wait for open
        loop_gate = 0; // close gate

        ev = keybrd_get_event();

#ifndef WITHOUT_LIGHT
        // sample adc, run frequently
        if (loop_count++ % 4 == 0) {
            // auto-dimming, by dividing adc range into 8 steps
            disp_bright_val = getADCResult(ADC_LIGHT) >> 5;
            // set floor of dimming range
            if (disp_bright_val < 4) {
                disp_bright_val = 4;
            }
        }
#endif
        // Read RTC
        ds_readburst();
        // parse RTC
        {
            rtc_hh_bcd = rtc_table[DS_ADDR_HOUR] & DS_MASK_HOUR24;
            rtc_mm_bcd = rtc_table[DS_ADDR_MINUTES] & DS_MASK_MINUTES;

            if (buzzer_bufer_hh != rtc_hh_bcd && buzzer_set_on)
            {
                buzzer_must_on = 1;
                buzzer_bufer_hh = rtc_hh_bcd;
            }
        }

        // keyboard decision tree
        switch (kmode) {
            case K_DEMO:
                if (ev != EV_NONE)
                {
                    kmode = K_NORMAL;
                }
                break;

            case K_SET_HOUR:
                blink_seg_01 = 1;
                if (ev == EV_S1_SHORT || (ev == EV_S1_LONG_PRESSED && blinker_fast)) {
                    ds_hours_incr();
                }
                else if (ev == EV_S2_SHORT) {
                    kmode = K_SET_MINUTE;
                }
                break;

            case K_SET_MINUTE:
                blink_seg_01 = 0;
                blink_seg_23 = 1;
                if (ev == EV_S1_SHORT || (ev == EV_S1_LONG_PRESSED && blinker_fast)) {
                    ds_minutes_incr();
                }
                else if (ev == EV_S2_SHORT) {
                    kmode = K_NORMAL;
                }
                break;

            case K_SEC_DISP:
                dmode = M_SEC_DISP;
                if (ev == EV_S1_SHORT) {
                    kmode = K_TIMER;
                }
                else if (ev == EV_S2_SHORT) {
                    ds_sec_zero();
                }
                break;

            case K_TIMER:
                dmode = M_TIMER;
                if (ev == EV_S1_SHORT) {
                    kmode = K_NORMAL;
                }
                else if (ev == EV_S2_SHORT) {
                    tmr_is_runing = !tmr_is_runing;
                }
                else if (ev == EV_S2_LONG) {
                    if (!tmr_is_runing)
                    {
                        tmr_ms_x10 = 0;
                        tmr_ss = 0;
                        tmr_mm = 0;
                    }
                }
                break;

            case K_NORMAL:
            default:
                blink_seg_01 = 0;
                blink_seg_23 = 0;

                dmode = M_NORMAL;

                if (ev == EV_S1_SHORT) {
                    kmode = K_SEC_DISP;
                }
                else if (ev == EV_S2_LONG) {
                    kmode = K_SET_HOUR;
                }
                else if (ev == EV_S1S2_LONG) {
                    buzzer_set_on = !buzzer_set_on;
                    buzzer_must_on = 1;
                }
        };

        // display execution tree

        clearTmpDisplay();

        switch (dmode) {
            case M_DEMO:
            {
                static uint8_t demo_cnt = LED_DEMO_0;

                if (blinker_slow)
                {
                    if (demo_cnt == LED_DEMO_5)
                    {
                        dmode = M_NORMAL;
                        kmode = K_NORMAL;
                        break;
                    }

                    blinker_slow = 0;
                    demo_cnt++;
                }

                filldisplay(0, demo_cnt, 0);
                filldisplay(1, demo_cnt, 0);
                filldisplay(2, demo_cnt, 0);
                filldisplay(3, demo_cnt, 0);

                colon_blink(blinker_fast);

                break;
            }

            case M_TIMER:
            {
                uint8_t bcd_01;
                uint8_t bcd_23;

                bcd_01 = ((tmr_ss / 10) << 4) | (tmr_ss % 10);

                if (tmr_mm == 0)
                {
                    bcd_23 = ((tmr_ms_x10 / 10) << 4) | (tmr_ms_x10 % 10);
                }
                else
                {
                    bcd_23 = bcd_01;
                    bcd_01 = ((tmr_mm / 10) << 4) | (tmr_mm % 10);
                }

                filldisplay(0, bcd_01 >> 4, 0);
                filldisplay(1, bcd_01 & 0x0F, 0);
                filldisplay(2, bcd_23 >> 4, 0);
                filldisplay(3, bcd_23 & 0x0F, 0);

                colon_blink(blinker_fast | !tmr_is_runing);

                break;
            }

            case M_NORMAL:
            {
                if (!blink_seg_01 || blinker_fast || ev == EV_S1_LONG_PRESSED)
                {
                    filldisplay(0, rtc_hh_bcd >> 4, 0);
                    filldisplay(1, rtc_hh_bcd & 0x0F, 0);
                }

                if (!blink_seg_23 || blinker_fast || ev == EV_S1_LONG_PRESSED)
                {
                    filldisplay(2, rtc_mm_bcd >> 4, 0);
                    filldisplay(3, rtc_mm_bcd & 0x0F, 0);
                }

                colon_blink(blinker_slow);

                break;
            }

            case M_SEC_DISP:
            {
                filldisplay(2,(rtc_table[DS_ADDR_SECONDS] >> 4) & (DS_MASK_SECONDS_TENS >> 4), 0);
                filldisplay(3, rtc_table[DS_ADDR_SECONDS] & DS_MASK_SECONDS_UNITS, 0);

                colon_blink(blinker_slow);

                break;
            }
        }

        __critical {
            updateTmpDisplay();
        }

        WDT_CLEAR();
    }
}
/* ------------------------------------------------------------------------- */
