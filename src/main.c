#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>

#include "timevalues.h"
#include "usb_keyboard.h"
#include "events.h"
#include "kbcomm.h"
#include "kbglue.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

//
// Private definitions and types
//

// Multimedia keys aren't listed in usb_keyboard.h.
// 
// The ones used here are from usb_hid_usages.txt, from
// http://www.freebsddiary.org/APC/usb_hid_usages
// 
// Translate.pdf is also included, from:
//
// http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
// mirror: http://www.hiemalis.org/~keiji/PC/scancode-translate.pdf
//
// It has some extra keys that are missing from usb_hid_usages, most
// notably play/pause.
//
// Add more to this list if you need them, and then add them to
// SwitchActionMap.

#define MediaKey(scancode) (0x1000 | scancode)

#define MODIFIER_KEYS_START 224
#define MODIFIER_KEYS_END 231

#define KEY_CTRL	224
#define KEY_SHIFT	225
#define KEY_ALT		226
#define KEY_GUI		227
#define KEY_LEFT_CTRL   224
#define KEY_LEFT_SHIFT	225
#define KEY_LEFT_ALT	226
#define KEY_LEFT_GUI	227
#define KEY_RIGHT_CTRL	228
#define KEY_RIGHT_SHIFT	229
#define KEY_RIGHT_ALT	230
#define KEY_RIGHT_GUI	231

// Wrapping a key in MediaKey makes it send as a "consumer" key (0x0C / 12
// in the above tables).
#define KEY_VOLUME_UP MediaKey(0xe9)
#define KEY_VOLUME_DOWN MediaKey(0xea)
#define KEY_VOLUME_MUTE MediaKey(0xe2) // no effect on Nexus 7
#define KEY_SLEEP MediaKey(0x32)
#define KEY_POWER MediaKey(0x30) // Nexus 7: sending KEY_POWER shows the power-off menu; holding KEY_SLEEP does the same
#define KEY_PLAYPAUSE MediaKey(0xcd)
#define KEY_STOP MediaKey(0xb7)
#define KEY_PREV MediaKey(0xb6)
#define KEY_NEXT MediaKey(0xb5)
#define KEY_REWIND MediaKey(0xb4)
#define KEY_FASTFORWARD MediaKey(0xb3)
#define KEY_WWWHOME MediaKey(0x223) // Nexus 7: same as device home button
#define KEY_WWWSEARCH MediaKey(0x221) // Nexus 7: this is the same as the hardware search button on many devices, but note that it triggers upon release, not press

// You probably won't need or want to change anything after this
// line.

// Number of consecutive ticks that a switch has to maintain the same
// value in order to register a keypress.
//
// Calculated the same way as LongPressTime, and affected by
// Timer0Overflow in the same way as well.
static uint8_t DebounceTimeLimitMS = 12;

// Amount to report the mouse moves for each quadrature change interrupt.
static uint8_t const MouseMoveAmountLowSpeed = 0x04;
static uint8_t const MouseMoveAmountMedSpeed = 0x08;
static uint8_t const MouseMoveAmountHighSpeed = 0x10;

static uint8_t const MouseCooldownMedSpeedStart =  8; // start highspeed when cooldown hits this value
static uint8_t const MouseCooldownHighSpeedStart = 16; // start highspeed when cooldown hits this value
static uint8_t const MouseCooldownHighSpeedCap = 75;
static uint8_t const MouseCooldownMovementStep = 3; // increment by this amount with each mouse movement; decrement by 1

//
// End of user-configurable stuff.
//


#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))
#define LED_CONFIG	(DDRD |= (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))
#define LED_ON		(PORTD |= (1<<6))
#define LED_TOGGLE  { if (PORTD & (1<<6)) { LED_OFF; } else { LED_ON; } }

//
// Constants
//


//
// Interrupt state
//

// Whether timer0 fired or not.  Set in ISR, and cleared in main.
static volatile uint8_t _timer0_fired;

// Mouse
static volatile uint8_t _mouse_quadrature_x_1_fired;
static volatile uint8_t _mouse_quadrature_x_2_fired;
static volatile uint8_t _mouse_quadrature_y_1_fired;
static volatile uint8_t _mouse_quadrature_y_2_fired;
static volatile uint8_t _mouse_button_fired;


//
// Functions
//

static void setup(void) {


    LED_CONFIG;
    LED_ON;
    
    // Clear the watchdog
    MCUSR &= ~_BV(WDRF);
    wdt_disable();
    wdt_reset();
    
    // 16 MHz clock speed
	CPU_PRESCALE(0);

    // Configure PORTB[0:6] as inputs.
    DDRB = 0x00;
    // Turn on internal pull-ups on PORTB[0:6].  This means we will read
    // them as logic high when the switches are open.  Logic low means
    // the switch is pressed (ie. active low).
    PORTB = 0x7f;

    LED_OFF;
    
	// Initialize USB, and then wait for the host to set
	// configuration.  If the Teensy is powered without a PC connected
	// to the USB port, this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

    LED_ON;
    
	// Wait an extra second for the PC's operating system to load
	// drivers and do whatever it does to actually be ready for input.
	// Show a little light show during this time.
    for(uint8_t i = 0; i < 5; i++) {
        _delay_ms(180);
        LED_OFF;
        _delay_ms(20);
        LED_ON;
    }
    
    cli();
    
    // PORTD[0:3] as quadrature inputs (pull-ups in case mouse is disconnected, but it always sends logic high/low)
    DDRD &= ~0x0f;
    PORTD |= 0x0f;

    // External interrupts for mouse quadrature inputs on PORTD[0:3]
    EICRA = 0x55; // int3:0: trigger on any edge change
    EIMSK |= 0x0f; // enable int3:0
    EIFR &= ~0x0f; // clear int3:0 flags
    _mouse_quadrature_x_1_fired = 0;
    _mouse_quadrature_x_2_fired = 0;
    _mouse_quadrature_y_1_fired = 0;
    _mouse_quadrature_y_2_fired = 0;

    // PORTE6 as button input (requires pull-up)
    DDRE &= ~0x40;
    PORTE |= 0x40;

    // External interrupt for PORTE6
    EICRB = 0x10; // int6: trigger on any edge change
    EIMSK |= 0x40; // enable int6
    EIFR &= ~0x40; // clear int6 flags
    _mouse_button_fired = 0;

    // Configure timer 0 to give us ticks
	TCCR0A = 0x00;
	TCCR0B = TVTimer0Overflow & 0x07;
	TIMSK0 = (1<<TOIE0); // use the overflow interrupt only
    _timer0_fired = 0;

    kb_setup();

    timer1_setup();
}

static int anything_fired(void) {
    return _timer0_fired || _mouse_quadrature_x_1_fired || _mouse_quadrature_x_2_fired || _mouse_quadrature_x_1_fired || _mouse_quadrature_y_2_fired || kb_isr_fired() || _mouse_button_fired;
}

static void run(void) {
    /* char buf[100]; */
    /* _delay_ms(100); */

    /* uint32_t start = timer1_read_ms(); */
    /* snprintf(buf, 100, "start %ld (%d)\n", start, timer1_read()); */
    /* usb_keyboard_send_message(buf); */

    /* _delay_ms(500); */
    /* uint32_t end = timer1_read_ms(); */

    /* snprintf(buf, 100, "%ld (%d) - %ld = %ld ms\n", end, timer1_read(), start, end - start); */
    /* usb_keyboard_send_message(buf); */
    
    wdt_reset();
    wdt_enable(WDTO_1S);

    uint8_t const DebounceTickLimit = DebounceTimeLimitMS / TVMillisPerTickTimer0;

    // mouse button debounce state
    uint8_t mouse_current_button = 0;
    uint8_t mouse_button_debounce_ticks = DebounceTickLimit + 1;

    // mouse "acceleration"
    uint8_t mouse_cooldown_ticks_x = 0, mouse_cooldown_ticks_y = 0;

    kg_begin();

	for(;;) {        
        uint8_t timer0_fired = 0;
        uint8_t mouse_quadrature_x_1_fired = 0;
        uint8_t mouse_quadrature_x_2_fired = 0;
        uint8_t mouse_quadrature_y_1_fired = 0;
        uint8_t mouse_quadrature_y_2_fired = 0;
        uint8_t mouse_button_fired = 0;
        
        // Watch for interrupts, and sleep if nothing has fired.
        cli();
        while(!anything_fired()) {
            set_sleep_mode(SLEEP_MODE_IDLE);
            sleep_enable();
            // It's safe to enable interrupts (sei) immediately before
            // sleeping.  Interrupts can't fire until after the
            // following instruction has executed, so there's no race
            // condition between re-enabling interrupts and sleeping.
            sei();
            sleep_cpu();
            sleep_disable();
            cli();
        }

        timer0_fired = _timer0_fired;
        mouse_quadrature_x_1_fired = _mouse_quadrature_x_1_fired;
        mouse_quadrature_x_2_fired = _mouse_quadrature_x_2_fired;
        mouse_quadrature_y_1_fired = _mouse_quadrature_y_1_fired;
        mouse_quadrature_y_2_fired = _mouse_quadrature_y_2_fired;
        mouse_button_fired = _mouse_button_fired;

        _timer0_fired = 0;
        _mouse_quadrature_x_1_fired = 0;
        _mouse_quadrature_x_2_fired = 0;
        _mouse_quadrature_y_1_fired = 0;
        _mouse_quadrature_y_2_fired = 0;
        _mouse_button_fired = 0;

        sei();

        // Keyboard

        kb_postisr();

        //
        // Mouse button
        //

        int8_t mouse_button_changed = 0;

        if (timer0_fired) {
            wdt_reset();

            // dispatch to listeners
            event_dispatch(EVENT_TYPE_TICK, NULL);

            // mouse debounce

            if (mouse_button_debounce_ticks < DebounceTickLimit) {
                mouse_button_debounce_ticks++;

                if (mouse_button_debounce_ticks == DebounceTickLimit) {
                    mouse_button_changed = 1;
                    mouse_current_button = (PINE & _BV(6)) ? 0x00 : 0x01;
                }
            }

            if (mouse_cooldown_ticks_x != 0) {
                mouse_cooldown_ticks_x--;
            }
            if (mouse_cooldown_ticks_y != 0) {
                mouse_cooldown_ticks_y--;
            }
        }

        //
        // Mouse movement
        //

        // If a quadrature pair doesn't match, then the one whose interrupt fired has a
        // leading edge, indicating that the mouse is moving in that direction.

        int8_t delta_x = 0;
        int8_t delta_y = 0;

        uint8_t mouse_speed_x = (mouse_cooldown_ticks_x >= MouseCooldownHighSpeedStart) ? MouseMoveAmountHighSpeed : ((mouse_cooldown_ticks_x >= MouseCooldownMedSpeedStart) ? MouseMoveAmountMedSpeed : MouseMoveAmountLowSpeed);
        uint8_t mouse_speed_y = (mouse_cooldown_ticks_y >= MouseCooldownHighSpeedStart) ? MouseMoveAmountHighSpeed : ((mouse_cooldown_ticks_y >= MouseCooldownMedSpeedStart) ? MouseMoveAmountMedSpeed : MouseMoveAmountLowSpeed);
        
        if (mouse_quadrature_x_1_fired) {
            uint8_t leading = (!(PIND & _BV(0)) != !(PIND & _BV(1)));
            if (leading) {
                delta_x += mouse_speed_x;
            } else {
                delta_x -= mouse_speed_y;
            }
        }

        if (mouse_quadrature_x_2_fired) {
            uint8_t leading = (!(PIND & _BV(0)) != !(PIND & _BV(1)));
            if (leading) {
                delta_x -= mouse_speed_x;
            } else {
                delta_x += mouse_speed_y;
            }
        }

        if (mouse_quadrature_y_1_fired) {
            uint8_t leading = (!(PIND & _BV(2)) != !(PIND & _BV(3)));
            if (leading) {
                delta_y -= mouse_speed_x;
            } else {
                delta_y += mouse_speed_y;
            }
        }

        if (mouse_quadrature_y_2_fired) {
            uint8_t leading = (!(PIND & _BV(2)) != !(PIND & _BV(3)));
            if (leading) {
                delta_y += mouse_speed_x;
            } else {
                delta_y -= mouse_speed_y;
            }
        }

        if (mouse_button_fired) {
            mouse_button_debounce_ticks = 0;
        }

        if (delta_x != 0 || delta_y != 0 || mouse_button_changed) {
            if (delta_x != 0) {
                if (mouse_cooldown_ticks_x < MouseCooldownHighSpeedCap - MouseCooldownMovementStep) {
                    mouse_cooldown_ticks_x += MouseCooldownMovementStep;
                }
            }
            if (delta_y != 0) {
                if (mouse_cooldown_ticks_y < MouseCooldownHighSpeedCap - MouseCooldownMovementStep) {
                    mouse_cooldown_ticks_y += MouseCooldownMovementStep;
                }
            }
        
            usb_mouse_send(mouse_current_button, delta_x, delta_y);
        }
    }
}

int main(void) {
    setup();
    run();
    return 0;
}


// Timer 0 overflow interrupt handler.
ISR(TIMER0_OVF_vect) {
    _timer0_fired = 1;
}

ISR(INT0_vect) {
    _mouse_quadrature_x_1_fired = 1;
}

ISR(INT1_vect) {
    _mouse_quadrature_x_2_fired = 1;
}

ISR(INT2_vect) {
    _mouse_quadrature_y_1_fired = 1;
}

ISR(INT3_vect) {
    _mouse_quadrature_y_2_fired = 1;
}

ISR(INT6_vect) {
    _mouse_button_fired = 1;
}
