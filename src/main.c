#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdint.h>

#include "usb_keyboard.h"

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

#define MediaKey(scancode) (0x1000 | scancode)
#define IsMediaKey(scancode) (0x1000 & scancode)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

#define KEY_VOLUME_UP MediaKey(0xe9)
#define KEY_VOLUME_DOWN MediaKey(0xea)
#define KEY_VOLUME_MUTE MediaKey(0xe2) // no effect on Nexus 7
#define KEY_SLEEP MediaKey(0x32)
#define KEY_PLAYPAUSE MediaKey(0xcd)
#define KEY_PREV MediaKey(0xb6)
#define KEY_NEXT MediaKey(0xb5)

typedef struct {
    uint8_t state; // pin state
    uint16_t count; // how many ticks it has been in this state
} PinState;

typedef enum {
    DirectionCCW,
    DirectionCW
} Direction;

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))
#define LED_CONFIG	(DDRD |= (1<<6))
#define LED_OFF		(PORTD &= ~(1<<6))
#define LED_ON		(PORTD |= (1<<6))

typedef struct {
    uint16_t *press_keys;
    uint16_t *long_press_keys;
} SwitchAction;

// Number of ticks that must pass before a held key is treated as a
// long press.
static uint16_t const LongPressTime = 20;

#ifndef NULL
#define NULL ((void *)0)
#endif

// Specify NULL instead of an array to not send any keys when that
// switch is pressed.
//
// End each array with 0 to indicate the end of the array.
//
// Example: Pressing this button would send shift, 2, 3, and
// play/pause, resulting in the characters @# and the media
// play/pausing.
//
// { ((uint16_t[]){ KEY_2, KEY_SHIFT, KEY_3, KEY_PLAYPAUSE, 0 }),
//   ((uint16_t[]){ KEY_W, 0 }) },   
//
// Long presses behave the following way:
//
//   press = NULL, long_press = NULL:
//
//     No action when pressed/released
//
//   press = keys, long_press = NULL:
//
//     When switch is active, the keys are pressed and held until the
//     switch is released.
//
//   press = keys, long_press = keys:
//
//     When switch is active, nothing happens until LongPressTime
//     ticks have passed; then the long-press keys are pressed and
//     held until the switch is released.  If the switch is released
//     before LongPressTime, then the normal keys are sent briefly
//     (ie. a single keypress).
//
//   press = NULL, long_press = keys:
//
//     Same as previous but nothing happens if the switch is released
//     before LongPressTime.
// 
static SwitchAction const SwitchActionMap[7] = {
    // PORTB0 = S2
    { ((uint16_t[]){ KEY_2, 0 }),
      ((uint16_t[]){ KEY_W, 0 }) },   

    // PORTB1 = A (dial; ignored)
    { NULL, NULL },              

    // PORTB2 = S1        
    { (uint16_t[]){ KEY_1, 0 },     
      (uint16_t[]){ KEY_Q, KEY_SHIFT, 0 } },

    // PORTB3 = S5
    { (uint16_t[]){ KEY_5, 0 },     
      (uint16_t[]){ KEY_T, KEY_SHIFT, 0 } },

    // PORTB4 = S4
    { (uint16_t[]){ KEY_4, 0 },     
      (uint16_t[]){ KEY_R, KEY_SHIFT, 0 } },

    // PORTB5 = B (dial; ignored)
    { NULL, NULL },              

    // PORTB6 = S3 */
    { (uint16_t[]){ KEY_3, 0 },
      (uint16_t[]){ KEY_E, KEY_SHIFT, 0 } }
};

static uint16_t const DialCCWKey = KEY_VOLUME_DOWN;
static uint16_t const DialCWKey = KEY_VOLUME_UP;

static uint8_t const DialA = 1; // PORTB2
static uint8_t const DialB = 5; // PORTB5


// Timer 0 clock select (prescaling; controls TCCR0B[2:0] aka CS0[2:0]).
//
// This controls how fast the system ticks.  We need to debounce
// before registering a keypress, so this should be reasonably fast in
// order to feel responsive.  See also DebounceTickLimit, which should
// be decreased if you increase the tick frequency.
//
//   0x05; // clkIO/1024 -> 61 Hz
//   0x04; // clkIO/256 -> 244.14 Hz
//   0x03; // clkIO/64 -> 976.6 Hz
static uint8_t const Timer0Overflow = 0x04;

// Number of consecutive ticks that a switch has to maintain the same
// value in order to register a keypress.
static uint8_t const DebounceTickLimit = 3;

// Interrupt communication.
static volatile uint8_t _timer0_fired;
    // Default state: all high = nothing pressed
static volatile uint8_t _raw_switches_state;

static PinState switch_debounce_states[7];
static uint8_t debounced_switches = 0x7f;
static uint8_t long_press_switches = 0x7f;

static void setup(void) {
    // 16 MHz clock speed
	CPU_PRESCALE(0);

    // Configure PORTB[0:6] as inputs.
    DDRB = 0x00;
    // Turn on internal pull-ups on PORTB[0:6].  This means we will read
    // them as logic high when the switches are open.  Logic low means
    // the switch is pressed (ie. active low).
    PORTB = 0x7f;

    LED_CONFIG;
    LED_OFF;
    
    for (uint8_t i = 0; i < 7; i++) {
        switch_debounce_states[i].state = 1;
        switch_debounce_states[i].count = 0;
    }

	// Initialize USB, and then wait for the host to set
	// configuration.  If the Teensy is powered without a PC connected
	// to the USB port, this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;
    
	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

    cli();
    
    // Configure timer 0 to give us ticks
	TCCR0A = 0x00;
	TCCR0B = Timer0Overflow & 0x07;
	TIMSK0 = (1<<TOIE0); // use the overflow interrupt only
    _timer0_fired = 0;
}

static void update_debounced_state(uint8_t raw_switches_state) {
    for(int i = 0; i < 7; i++) {
        uint8_t key_val = (raw_switches_state >> i) & 0x01;
        if (key_val != switch_debounce_states[i].state) {
            // Any time the read value doesn't match our debounce
            // state, we reset the count.
            
            switch_debounce_states[i].count = 0;
            switch_debounce_states[i].state = key_val;

        } else if (switch_debounce_states[i].count < MAX(DebounceTickLimit, LongPressTime)) {
            // If it DOES match and we haven't reached the debounce
            // tick limit, we increment it.
            
            switch_debounce_states[i].count++;
            
            if (switch_debounce_states[i].count == DebounceTickLimit) {
                // Once we've hit the tick limit, we register that as
                // a keypress state change.
                
                // Clear the bit for this switch, and then set it to
                // the new, debounced value.
                
                debounced_switches &= ~(0x01 << i);
                debounced_switches |= (key_val << i);

                if (key_val == 1) {
                    // We need to release the long press for this
                    // switch immediately upon release.
                    long_press_switches |= (key_val << i);
                }
            }

            if ((switch_debounce_states[i].count == LongPressTime) &&
                (key_val == 0)) {
                // The tick limit for a long press has been reached,
                // so we register this as a long button press.

                long_press_switches &= ~(0x01 << i);
            }
        }
    }
}

static void press(uint16_t encoded_key) {
    if (IsMediaKey(encoded_key)) {
        usb_media_press(encoded_key & 0xfff);
    } else {
        usb_keyboard_press(encoded_key & 0xff, 0);
    }
}

static void media_key_change(uint16_t key, uint8_t newstate) {
    uint8_t i, free_index = 255;

    for(i = 0; i < 4; i++) {
        if (media_keys[i] == key) {
            if (newstate) {
                // The key is already on; we can stop altogether
                free_index = 255;
                break;
            } else {
                media_keys[i] = 0;
            }
        }
        if (newstate && !media_keys[i] && free_index == 255) {
            free_index = i;
        }
    }

    if (newstate && free_index < 4) {
        // If newstate but free_index == 255, then we either don't
        // have room in the buffer for the new key, or the key is
        // already pressed.  Either way we have no action to take.
        //
        // This path, however, is when we found an empty slot in the
        // key buffer, so we put the key into it.

        media_keys[free_index] = key;
    }
}

static void basic_key_change(uint8_t key, uint8_t newstate) {
    uint8_t i, free_index = 255;

    if (key >= MODIFIER_KEYS_START && key <= MODIFIER_KEYS_END) {
        // modifier keys are stored as bitfields
        uint8_t affected_field = key & 0x07; // 0b00000xxx: 227 (KEY_GUI) => 0b00000011 (3)
        uint8_t mask = (newstate ? 0x01 : 0) << affected_field; // 1 << 3 => 0b00001000 (or 0 if turning off)

        keyboard_modifier_keys &= ~(0x01 << affected_field);
        keyboard_modifier_keys |= mask;
        
        return;
    }
        
    for(i = 0; i < 6; i++) {
        if (keyboard_keys[i] == key) {
            if (newstate) {
                // The key is already on; we can stop altogether
                free_index = 255;
                break;
            } else {
                keyboard_keys[i] = 0;
            }
        }
        if (newstate && !keyboard_keys[i] && free_index == 255) {
            free_index = i;
        }
    }

    if (newstate && free_index < 4) {
        // If newstate but free_index == 255, then we either don't
        // have room in the buffer for the new key, or the key is
        // already pressed.  Either way we have no action to take.
        //
        // This path, however, is when we found an empty slot in the
        // key buffer, so we put the key into it.

        keyboard_keys[free_index] = key;
    }
}

static void send_keys(uint16_t *keys, uint8_t pressed) {
    uint8_t i;
    
    for (i = 0; keys[i]; i++) {
        uint16_t encoded_key = keys[i];
        
        if (IsMediaKey(encoded_key)) {
            media_key_change(encoded_key & 0xfff, pressed);
        } else {
            basic_key_change(encoded_key & 0xff, pressed);
        }
    }
    usb_keyboard_send();
    usb_media_send();
}

static void press_keys(uint16_t *keys) {
    send_keys(keys, 1);
}

static void release_keys(uint16_t *keys) {
    send_keys(keys, 0);
}

static void run(void) {
    uint8_t last_pressed_keys = 0x7f;
    uint8_t last_long_pressed_keys = 0x7f;
    
    uint8_t dial_moving = 0;
    uint8_t dial_position = (PINB >> DialA) & 0x01;
    Direction dial_direction = DirectionCCW;
    
	for(;;) {        
        uint8_t timer0_fired = 0;
        uint8_t raw_switches_state = 0x7f;
        
        // Watch for interrupts, and sleep if nothing has fired.
        cli();
        while(!_timer0_fired) {
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
        raw_switches_state = _raw_switches_state;
        _timer0_fired = 0;

        sei();

        if (timer0_fired) {
            update_debounced_state(raw_switches_state);
            uint8_t changed_keys = last_pressed_keys ^ debounced_switches;

            // Process normal keys
            for(int i = 0; i < 7; i++) {
                // A switch is pressed if it's logic low.
                if ((((debounced_switches >> i) & 0x01) == 0) &&
                    ((changed_keys >> i) & 0x01)) {

                    uint16_t *action_keys = SwitchActionMap[i].press_keys;
                    if (action_keys) {
                        press_keys(action_keys);
                        release_keys(action_keys);
                    }
                }
            }

            // Process dial
            if (((debounced_switches >> DialA) & 0x01) != ((debounced_switches >> DialB) & 0x01)) {
                // The dial inputs are different from one another, so
                // it's moving now.
                dial_moving = 1;
                dial_direction = (((debounced_switches >> DialA) & 0x01) != dial_position) ? DirectionCW : DirectionCCW;
            } else if (dial_moving) {
                // Dial was moving but now has stopped, as indicated
                // by the fact that the two inputs now have the same
                // value.
                dial_moving = 0;
                if (((debounced_switches >> DialA) & 0x01) != dial_position) {
                    // Dial moved to new position.
                    dial_position = ((debounced_switches >> DialA) & 0x01);
                    if (dial_direction == DirectionCW) {
                        press(DialCWKey);
                    } else {
                        press(DialCCWKey);
                    }
                } else {
                    // Dial returned to old position.  (Nothing to
                    // do.)
                }
            } else if (((debounced_switches >> DialA) & 0x01) != dial_position) {
                // Dial isn't moving, and A and B positions match, but
                // they don't match what we expect so we missed a full
                // click and need to update our internal state to
                // match.
                dial_position = ((debounced_switches >> DialA) & 0x01);
            }

                
            last_pressed_keys = debounced_switches;
            last_long_pressed_keys = long_press_switches;
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
    _raw_switches_state = PINB & 0x7f;
}
