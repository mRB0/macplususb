#include "kbcomm.h"
#include "events.h"
#include "timevalues.h"
#include "usb_keyboard.h"


#include <stdint.h>
#include <string.h>

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <util/delay.h>

#define HOLD_FOR_RECEIVE_TICK_COUNT 1


static volatile uint8_t _xfer_byte;
static volatile uint8_t _reading; // 0 = reading from keyboard into _xfer_byte; 1 = writing from _xfer_byte to keyboard
static volatile uint8_t _count; // number of bits read or written so far
static volatile uint8_t _completed, _active, _hold_for_receive;


static void (*_read_completion)(uint8_t result, uint8_t data);
static void (*_write_completion)(uint8_t result);

static uint8_t DBG_usb_keys_for_numbers[] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };

static uint16_t _ticks_until_reset;
static uint16_t _ticks_since_last_comm;

static void _tick_handler(void *context, event_type_t event_type, void *event_args);

void kb_setup(void) {
    // Clock always input, pull-up enabled
    KB_CLK_DDR &= ~_BV(KB_CLK_BIT);
    KB_CLK_PORT |= _BV(KB_CLK_BIT);

    // Data starts as input, pull-up enabled
    KB_DATA_PORT |= _BV(KB_DATA_BIT);
    KB_DATA_DDR &= ~_BV(KB_DATA_BIT);

    // External interrupt for PORTE7
    EICRB = 0x80; // int7: trigger on falling edge

    EIMSK &= ~0x80; // disable int7 until required

    _ticks_until_reset = (uint8_t)((uint16_t)500 / TVMillisPerTick);

    event_register_handler(EVENT_TYPE_TICK, _tick_handler, NULL);
}

#define ISR_CALLS_PER_BYTE 8

static void _tick_handler(void *context, event_type_t event_type, void *event_args) {
    if (_hold_for_receive) {
        _hold_for_receive--;
        if (_hold_for_receive == 0) {
            // data line to input high
            KB_DATA_PORT |= _BV(KB_DATA_BIT);
            KB_DATA_DDR &= ~_BV(KB_DATA_BIT);

            EIFR &= ~0x80; // clear int7 flags
            EICRB = 0x80; // int7: trigger on falling edge
            EIMSK |= 0x80; // enable int7
        }
    }

    if (!_completed) {
        _ticks_since_last_comm++;
        if (_ticks_since_last_comm > _ticks_until_reset) {
            usb_keyboard_press(KEY_1, MODIFIER_KEY_LEFT_SHIFT);
            _completed = 1;
            _active = 0;

            void (*read_completion)(uint8_t result, uint8_t data) = _read_completion;
            void (*write_completion)(uint8_t result) = _write_completion;
            _read_completion = NULL;
            _write_completion = NULL;

            if (_reading && read_completion) {
                read_completion(1, 0);
            } else if (!_reading && write_completion) {
                write_completion(1);
            }
        }
    }
}


void kb_readbyte(void (*read_completed)(uint8_t result, uint8_t data)) {
    EIMSK &= ~0x80; // disable int7
    _ticks_since_last_comm = 0;

    _read_completion = read_completed;

    _xfer_byte = 0x00;
    _count = 0;
    _reading = 1;
    _completed = 0;
    _active = 1;

    // we have problems if we expect a received byte too soon, so we wait for the next tick before advising the keyboard of our desire to receive another byte
    _hold_for_receive = HOLD_FOR_RECEIVE_TICK_COUNT;
}

void kb_writebyte(uint8_t data, void (*write_completed)(uint8_t result)) {

    EIMSK &= ~0x80; // disable int7
    _ticks_since_last_comm = 0;
    usb_keyboard_press(KEY_MINUS, 0);

    _write_completion = write_completed;

    _xfer_byte = data;
    _count = 0;
    _reading = 0;
    _completed = 0;
    _active = 1;

    // data line to output low
    KB_DATA_PORT &= ~_BV(KB_DATA_BIT);
    KB_DATA_DDR |= _BV(KB_DATA_BIT);

    EICRB = 0x80; // int7: trigger on falling edge
    EIFR &= ~0x80; // clear int7 flags
    EIMSK |= 0x80; // enable int7
}

void kb_postisr(void) {
    
    void (*read_completion)(uint8_t result, uint8_t data) = NULL;
    void (*write_completion)(uint8_t result) = NULL;

    uint8_t intr_state = SREG;
    cli();
    if (_completed && _active) {
        _active = 0;
        SREG = intr_state;
        usb_keyboard_press(KEY_SEMICOLON, 0);
        cli();
        _ticks_since_last_comm = 0;

        // end of byte
        _count = ISR_CALLS_PER_BYTE + 1;

        if (_reading) {
            read_completion = _read_completion;
            
            _read_completion = NULL;
        } else {
            write_completion = _write_completion;

            _write_completion = NULL;
        }
    }
    SREG = intr_state;

    if (read_completion) {
        read_completion(0, _xfer_byte);
    }
    if (write_completion) {
        write_completion(0);
    }
}

uint8_t kb_isr_fired(void) {
    return (_completed && _active);
}

ISR(INT7_vect) {
    // if (_count >= ISR_CALLS_PER_BYTE) {
    //     return;
    // }

    if (!(KB_CLK_PIN & _BV(KB_CLK_BIT))) {
        if (_reading) {
            _xfer_byte = (_xfer_byte << 1) | ((KB_DATA_PIN & _BV(KB_DATA_BIT)) ? 0x01 : 0);
        } else {
            if (_xfer_byte & 0x80) {
                KB_DATA_PORT |= _BV(KB_DATA_BIT);
            } else {
                KB_DATA_PORT &= ~_BV(KB_DATA_BIT);
            }

            _xfer_byte <<= 1;
        }
        _count++;

        if (_count == ISR_CALLS_PER_BYTE) {
            EICRB = 0xC0; // int7: trigger on RISING edge (to release output)
        }
    } else {
        EIMSK &= ~0x80; // disable int7 until next call
        _completed = 1;
    }
}
