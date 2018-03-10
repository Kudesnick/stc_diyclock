#include <stdint.h>
#include "keybrd.h"
#include "hwconfig.h"

volatile keybrd_event_t event;

volatile __bit S1_LONG;
volatile __bit S1_PRESSED;
volatile __bit S2_LONG;
volatile __bit S2_PRESSED;

keybrd_event_t keybrd_get_event(void)
{
    keybrd_event_t ev = event;
    event = EV_NONE;
    if (ev == EV_NONE)
    {
        if (S1_PRESSED) ev = EV_S1_LONG_PRESSED;
        if (S2_PRESSED) ev = EV_S2_LONG_PRESSED;
        if (S1_PRESSED && S2_PRESSED) ev = EV_S1S2_LONG_PRESSED;
    }
    return ev;
}

void keybrd_routine(void)
{
    volatile static uint8_t debounce[NUM_SW];      // switch debounce buffer
    volatile static uint8_t switchcount[NUM_SW];
    #define SW_CNTMAX 80

    keybrd_event_t ev = EV_NONE;

    #define MONITOR_S(n) \
    { \
        uint8_t s = n - 1; \
        /* read switch positions into sliding 8-bit window */ \
        debounce[s] = (debounce[s] << 1) | SW ## n ; \
        if (debounce[s] == 0) { \
            /* down for at least 8 ticks */ \
            S ## n ## _PRESSED = 1; \
            if (!S ## n ## _LONG) { \
                switchcount[s]++; \
            } \
        } else { \
            /* released or bounced */ \
            if (S ## n ## _PRESSED) { \
                if (!S ## n ## _LONG) { \
                    ev = EV_S ## n ## _SHORT; \
                } \
                S ## n ## _PRESSED = 0; \
                S ## n ## _LONG = 0; \
                switchcount[s] = 0; \
            } \
        } \
        if (switchcount[s] > SW_CNTMAX) { \
            S ## n ## _LONG = 1; \
            switchcount[s] = 0; \
            ev = EV_S ## n ## _LONG; \
        } \
    }

    MONITOR_S(1);
    MONITOR_S(2);

    if (ev == EV_S1_LONG && S2_PRESSED) {
        S2_LONG = 1;
        switchcount[1] = 0;
        ev = EV_S1S2_LONG;
    } else if (ev == EV_S2_LONG && S1_PRESSED) {
        S1_LONG = 1;
        switchcount[0] = 0;
        ev = EV_S1S2_LONG;
    }
    if (event == EV_NONE) {
        event = ev;
    }
}
