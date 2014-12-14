#include "timevalues.h"

// Timer 0 clock select (prescaling; controls TCCR0B[2:0] aka
// CS0[2:0]).
//
// This controls how fast the system ticks.  We need to debounce
// before registering a keypress, so this should be reasonably fast in
// order to feel responsive.  See also DebounceTickLimit, which should
// be decreased if you increase the tick frequency.
//
//   0x05; // clkIO/1024 -> 61 Hz
//   0x04; // clkIO/256 -> 244.14 Hz
//   0x03; // clkIO/64 -> 976.6 Hz
uint8_t const TVTimer0Overflow = 0x04;
uint16_t const TVMillisPerTick = 4; // 1000 * 1 / 244.14 Hz

