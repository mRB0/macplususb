#include "kbglue.h"

#include "kbcomm.h"
#include "usb_keyboard.h"

#define CMD_MODEL 0x16
#define CMD_TRANSITION 0x10
#define CMD_INSTANT 0x14

static void _model_write_completed(uint8_t result);
static void _model_read_completed(uint8_t result, uint8_t data);

static void _instant_write_completed(uint8_t result);

static void _transition_write_completed(uint8_t result);
static void _transition_read_completed(uint8_t result, uint8_t data);

//

static uint8_t _check_result(uint8_t result) {
    if (result != 0) {
        // start over
        usb_keyboard_press(KEY_2, MODIFIER_KEY_LEFT_SHIFT);

        kb_writebyte(CMD_MODEL, _model_write_completed);
    }
    return result;
}

// reads

static void _dbg_send_data(uint8_t data) {
    for (int i = 0; i < 8; i++) {
        uint8_t ltr = (data & 0x80) ? KEY_1 : KEY_0;
        data <<= 1;
        usb_keyboard_press(ltr, 0);
    }
    usb_keyboard_press(KEY_ENTER, 0);

}

static void _model_read_completed(uint8_t result, uint8_t data) {
    if (_check_result(result)) {
        return;
    }

    _dbg_send_data(data);
    // don't actually care about model
    kb_writebyte(CMD_TRANSITION, _transition_write_completed);
}

static void _transition_read_completed(uint8_t result, uint8_t data) {
    if (_check_result(result)) {
        return;
    }

    _dbg_send_data(data);
    if (data == 0x7b) {
        // null; wait for next transition
        kb_writebyte(CMD_TRANSITION, _transition_write_completed);
    } else if (data == 0x79) {
        // keypad; perform instant
        kb_writebyte(CMD_INSTANT, _instant_write_completed);
    } else {
        // process key in data and request next key transition
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
    usb_keyboard_press(KEY_PERIOD, 0);
    if (_check_result(result)) {
        return;
    }

    kb_readbyte(_model_read_completed);
}

void kg_begin(void) {
    kb_writebyte(CMD_MODEL, _model_write_completed);
}
