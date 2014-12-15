#include "kbglue.h"

#include "kbcomm.h"
#include "usb_keyboard.h"
#include "keymap.h"

#include <string.h>

#define CMD_MODEL 0x16
#define CMD_TRANSITION 0x10
#define CMD_INSTANT 0x14

static void _model_write_completed(uint8_t result);
static void _model_read_completed(uint8_t result, uint8_t data);

static void _instant_write_completed(uint8_t result);

static void _transition_write_completed(uint8_t result);
static void _transition_read_completed(uint8_t result, uint8_t data);

static uint8_t _expecting_keypad_result = 0;

static uint8_t _keys_in_buffer[128]; // index is a key scancode; this points to the location in keyboard_keys for each key when pressed and contains 0xff when not pressed


//

static uint8_t _check_result(uint8_t result) {
    if (result != 0) {
        // start over
        kb_writebyte(CMD_MODEL, _model_write_completed);
    }
    return result;
}

// processing

static void _dbg_send_data(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        uint8_t ltr = (data & 0x80) ? KEY_1 : KEY_0;
        data <<= 1;
        usb_keyboard_press(ltr, 0);
    }
    usb_keyboard_press(KEY_ENTER, 0);

}

static void _process_key(uint8_t data) {
    uint8_t scancode = data & 0x7f;
    uint8_t key_up = !!(data & 0x80);

    if (!_expecting_keypad_result) {
        if (key_up) {
            // key up
            if (_keys_in_buffer[scancode] != 0xff) {
                keyboard_keys[_keys_in_buffer[scancode]] = 0;
                _keys_in_buffer[scancode] = 0xff;
            } // else: key wasn't actually pressed
        } else {
            // key down
            if (_keys_in_buffer[scancode] == 0xff) {
                if (!AppleScancodeToUSBKey[scancode]) {
                    _dbg_send_data(data);
                }

                for(uint8_t i = 0; i < 6; i++) {
                    if (keyboard_keys[i] == 0) {
                        _keys_in_buffer[scancode] = i;
                        keyboard_keys[i] = AppleScancodeToUSBKey[scancode];
                        break;
                    }
                }
            } // else: key is already pressed
        }

    } else {

        _expecting_keypad_result = 0;
    }

    usb_keyboard_send();
}

// reads

static void _model_read_completed(uint8_t result, uint8_t data) {
    if (_check_result(result)) {
        return;
    }

    // don't actually care about model
    kb_writebyte(CMD_TRANSITION, _transition_write_completed);
}

static void _transition_read_completed(uint8_t result, uint8_t data) {
    if (_check_result(result)) {
        return;
    }

    if (data == 0x7b) {
        // null; wait for next transition
        kb_writebyte(CMD_TRANSITION, _transition_write_completed);
    } else if (data == 0x79) {
        // keypad; perform instant
        _expecting_keypad_result = 1;
        kb_writebyte(CMD_INSTANT, _instant_write_completed);
    } else {
        // process key in data and request next key transition
        _process_key(data);
        kb_writebyte(CMD_TRANSITION, _transition_write_completed);
    }
}

// writes

static void _instant_write_completed(uint8_t result) {
    if (_check_result(result)) {
        return;
    }

    // process result the same as transition read
    kb_readbyte(_transition_read_completed);
}

static void _transition_write_completed(uint8_t result) {
    if (_check_result(result)) {
        return;
    }

    kb_readbyte(_transition_read_completed);
}

static void _model_write_completed(uint8_t result) {
    if (_check_result(result)) {
        return;
    }

    kb_readbyte(_model_read_completed);
}

void kg_begin(void) {

    for(uint8_t i = 0; i < 128; i++) {
        _keys_in_buffer[i] = 0xff;
    }

    kb_writebyte(CMD_MODEL, _model_write_completed);
}
