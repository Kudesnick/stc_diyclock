#include "stc15.h"

typedef enum {
    EV_NONE,
    EV_S1_SHORT,
    EV_S1_LONG,
    EV_S1_LONG_PRESSED,
    EV_S2_SHORT,
    EV_S2_LONG,
    EV_S2_LONG_PRESSED,
    EV_S1S2_LONG,
    EV_S1S2_LONG_PRESSED,
    EV_TIMEOUT,
} keybrd_event_t;

keybrd_event_t keybrd_get_event(void);
void keybrd_routine(void);
