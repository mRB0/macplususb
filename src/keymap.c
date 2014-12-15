#include "keymap.h"
#include "usb_keyboard.h"

uint8_t const AppleScancodeToUSBKey[128] = {
    0,
    KEY_A,
    0,
    KEY_S,
    0,
    KEY_D,
    0,
    KEY_F,
    0,
    KEY_H,
    0,
    KEY_G,
    0,
    KEY_Z,
    0,
    KEY_X,

    // 0x10

    0,
    KEY_C,
    0,
    KEY_Y,
    0,
    0,
    0,
    KEY_B,
    0,
    KEY_Q,
    0,
    KEY_W,
    0,
    KEY_E,
    0,
    KEY_R,
    
    // 0x20

    0,
    KEY_Y,
    0,
    KEY_T,
    0,
    KEY_1,
    0,
    KEY_2,
    0,
    KEY_3,
    0,
    KEY_4,
    0,
    KEY_6,
    0,
    KEY_5,
    
    // 0x30

    0,
    KEY_EQUAL,
    0,
    KEY_9,
    0,
    KEY_7,
    0,
    KEY_MINUS,
    0,
    KEY_8,
    0,
    KEY_0,
    0,
    KEY_RIGHT_BRACE,
    0,
    KEY_O,
    
    // 0x40

    0,
    KEY_U,
    0,
    KEY_LEFT_BRACE,
    0,
    KEY_I,
    0,
    KEY_P,
    0,
    KEY_ENTER,
    0,
    KEY_L,
    0,
    KEY_J,
    0,
    KEY_QUOTE,
    
    // 0x50

    0,
    KEY_K,
    0,
    KEY_SEMICOLON,
    0,
    KEY_BACKSLASH,
    0,
    KEY_COMMA,
    0,
    KEY_SLASH,
    0,
    KEY_N,
    0,
    KEY_M,
    0,
    KEY_PERIOD,
    
    // 0x60

    0,
    KEY_TAB,
    0,
    KEY_SPACE,
    0,
    KEY_TILDE,
    0,
    KEY_BACKSPACE,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0, // command
    
    // 0x70

    0,
    0, // shift
    0,
    KEY_CAPS_LOCK,
    0,
    0, // option
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,    
};

uint8_t const AppleKeypadScancodeToUSBKey[128] = {
    0,
    0,
    0,
    KEYPAD_PERIOD,
    0,
    KEY_RIGHT,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    KEY_LEFT,
    0,
    KEY_NUM_LOCK,

    // 0x10
    
    0,
    KEY_DOWN,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    KEYPAD_ENTER,
    0,
    KEY_UP,
    0,
    KEYPAD_MINUS,
    0,
    0,

    // 0x20
    
    0,
    0,
    0,
    0,
    0,
    KEYPAD_0,
    0,
    KEYPAD_1,
    0,
    KEYPAD_2,
    0,
    KEYPAD_3,
    0,
    KEYPAD_4,
    0,
    KEYPAD_5,

    // 0x30
    
    0,
    KEYPAD_6,
    0,
    KEYPAD_7,
    0,
    0,
    0,
    KEYPAD_8,
    0,
    KEYPAD_9,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x40
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x50
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x60
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x70
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

uint8_t const AppleShiftedKeypadScancodeToUSBKey[128] = {
    // 0x00
    
    0,
    0,
    0,
    0,
    0,
    KEYPAD_ASTERIX,
    0,
    0,
    0,
    0,
    0,
    KEYPAD_PLUS,
    0,
    0,
    0,
    0,

    // 0x10
    
    0,
    KEY_EQUAL,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    KEYPAD_SLASH,
    0,
    0,
    0,
    0,

    // 0x20
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x30
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x40
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x50
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x60
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    // 0x70
    
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

};
