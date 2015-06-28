#ifndef TIME_VALUES_H_
#define TIME_VALUES_H_

#include "stdint.h"

extern uint8_t const TVTimer0Overflow;

// TODO: These two very similarly-named variables have different semantics.  Please fix them.


// TVMillisPerTickTimer0 is the approximate number of milliseconds per OVERFLOW (ie. interrupt fired) on timer0.
extern uint16_t const TVMillisPerTickTimer0;

// TVMillisPerTickTimer1 is the approximate number of milliseconds per OVERFLOW on timer1.
extern uint16_t const TVMillisPerTickTimer1;



void timer1_setup(void);

#define timer1_read() (TCNT1)
#define timer1_read_ms() (timer1_read() * TVMillisPerTickTimer1)

#endif
