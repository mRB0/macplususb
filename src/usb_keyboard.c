/* USB Keyboard Example for Teensy USB Development Board
 * http://www.pjrc.com/teensy/usb_keyboard.html
 * Copyright (c) 2009 PJRC.COM, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Version 1.0: Initial Release
// Version 1.1: Add support for Teensy 2.0


#define USB_SERIAL_PRIVATE_INCLUDE
#include "usb_keyboard.h"

#include <string.h>
 
/**************************************************************************
 *
 *  Configurable Options
 *
 **************************************************************************/

// You can change these to give your code its own name.
#define STR_MANUFACTURER        L"MfgName"
#define STR_PRODUCT             L"Keyboard"


// Mac OS-X and Linux automatically load the correct drivers.  On
// Windows, even though the driver is supplied by Microsoft, an
// INF file is needed to load the driver.  These numbers need to
// match the INF file.
#define VENDOR_ID               0x16C0
#define PRODUCT_ID              0x047C


// USB devices are supposed to implment a halt feature, which is
// rarely (if ever) used.  If you comment this line out, the halt
// code will be removed, saving 102 bytes of space (gcc 4.3.0).
// This is not strictly USB compliant, but works with all major
// operating systems.
#define SUPPORT_ENDPOINT_HALT



/**************************************************************************
 *
 *  Endpoint Buffer Configuration
 *
 **************************************************************************/

#define ENDPOINT0_SIZE          32

#define FIRST_ENDPOINT 2
#define LAST_ENDPOINT 4

#define KEYBOARD_INTERFACE      0
#define MEDIA_INTERFACE         1
#define MOUSE_INTERFACE         2
#define MOUSE_ENDPOINT          2
#define KEYBOARD_ENDPOINT       3
#define MEDIA_ENDPOINT          4
#define MOUSE_SIZE              4
#define MOUSE_BUFFER            EP_DOUBLE_BUFFER
#define KEYBOARD_SIZE           8
#define KEYBOARD_BUFFER         EP_DOUBLE_BUFFER
#define MEDIA_SIZE              8
#define MEDIA_BUFFER            EP_DOUBLE_BUFFER

static const uint8_t PROGMEM endpoint_config_table[] = {
    0,
    1, EP_TYPE_INTERRUPT_IN,  EP_SIZE(MOUSE_SIZE) | MOUSE_BUFFER,
    1, EP_TYPE_INTERRUPT_IN,  EP_SIZE(KEYBOARD_SIZE) | KEYBOARD_BUFFER,
    1, EP_TYPE_INTERRUPT_IN,  EP_SIZE(MEDIA_SIZE) | MEDIA_BUFFER,
};


/**************************************************************************
 *
 *  Descriptor Data
 *
 **************************************************************************/

// Descriptors are the data that your computer reads when it auto-detects
// this USB device (called "enumeration" in USB lingo).  The most commonly
// changed items are editable at the top of this file.  Changing things
// in here should only be done by those who've read chapter 9 of the USB
// spec and relevant portions of any USB class specifications!


static uint8_t const PROGMEM device_descriptor[] = {
    18,                                     // bLength
    1,                                      // bDescriptorType
    0x00, 0x02,                             // bcdUSB
    0,                                      // bDeviceClass
    0,                                      // bDeviceSubClass
    0,                                      // bDeviceProtocol
    ENDPOINT0_SIZE,                         // bMaxPacketSize0
    LSB(VENDOR_ID), MSB(VENDOR_ID),         // idVendor
    LSB(PRODUCT_ID), MSB(PRODUCT_ID),       // idProduct
    0x00, 0x13,                             // bcdDevice
    1,                                      // iManufacturer
    2,                                      // iProduct
    0,                                      // iSerialNumber
    1                                       // bNumConfigurations
};

// Keyboard Protocol 1, HID 1.11 spec, Appendix B, page 59-60
static uint8_t const PROGMEM keyboard_hid_report_desc[] = {
    0x05, 0x01,          // Usage Page (Generic Desktop),
    0x09, 0x06,          // Usage (Keyboard),
    0xA1, 0x01,          // Collection (Application),
        
    0x75, 0x01,          //   Report Size (1),
    0x95, 0x08,          //   Report Count (8),
    0x05, 0x07,          //   Usage Page (Key Codes),
    0x19, 0xE0,          //   Usage Minimum (224),
    0x29, 0xE7,          //   Usage Maximum (231),
    0x15, 0x00,          //   Logical Minimum (0),
    0x25, 0x01,          //   Logical Maximum (1),
    0x81, 0x02,          //   Input (Data, Variable, Absolute), ;Modifier byte
        
    0x95, 0x01,          //   Report Count (1),
    0x75, 0x08,          //   Report Size (8),
    0x81, 0x03,          //   Input (Constant),                 ;Reserved byte
        
    0x95, 0x05,          //   Report Count (5),
    0x75, 0x01,          //   Report Size (1),
    0x05, 0x08,          //   Usage Page (LEDs),
    0x19, 0x01,          //   Usage Minimum (1),
    0x29, 0x05,          //   Usage Maximum (5),
    0x91, 0x02,          //   Output (Data, Variable, Absolute), ;LED report
        
    0x95, 0x01,          //   Report Count (1),
    0x75, 0x03,          //   Report Size (3),
    0x91, 0x03,          //   Output (Constant),                 ;LED report padding
                
    0x95, 0x06,          //   Report Count (6),
    0x75, 0x08,          //   Report Size (8),
    0x15, 0x00,          //   Logical Minimum (0),
    0x25, 0x68,          //   Logical Maximum(104),
    0x05, 0x07,          //   Usage Page (Key Codes),
    0x19, 0x00,          //   Usage Minimum (0),
    0x29, 0x68,          //   Usage Maximum (104),
    0x81, 0x00,          //   Input (Data, Array),
        
    0xc0                 // End Collection
};

// Media keys: Modified version of above
static uint8_t const PROGMEM media_hid_report_desc[] = {
    0x05, 0x0c,          // Usage Page (Generic Desktop),
    0x09, 0x01,          // Usage (Keyboard),
    0xA1, 0x01,          // Collection (Application),
    
    0x95, 0x04,          //   Report Count (4),
    0x75, 0x10,          //   Report Size (16),
    0x15, 0x00,          //   Logical Minimum (0),
    0x26, 0x3c, 0x02,    //   Logical Maximum (0x23c),
    0x05, 0x0c,          //   Usage Page (Multimedia/Consumer),
    0x19, 0x00,          //   Usage Minimum (0),
    0x2a, 0x3c, 0x02,    //   Usage Maximum (0x23c),
    0x81, 0x00,          //   Input (Data, Array),
        
    0xc0                 // End Collection
};

static uint8_t const PROGMEM mouse_hid_report_desc[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop)
    0x09, 0x02,         // Usage (Mouse)
    0xA1, 0x01,         // Collection (Application)
    0x05, 0x09,         //   Usage Page (Button)
    0x19, 0x01,         //   Usage Minimum (Button #1)
    0x29, 0x03,         //   Usage Maximum (Button #3)
    0x15, 0x00,         //   Logical Minimum (0)
    0x25, 0x01,         //   Logical Maximum (1)
    0x95, 0x03,         //   Report Count (3)
    0x75, 0x01,         //   Report Size (1)
    0x81, 0x02,         //   Input (Data, Variable, Absolute)
    0x95, 0x01,         //   Report Count (1)
    0x75, 0x05,         //   Report Size (5)
    0x81, 0x03,         //   Input (Constant)
    0x05, 0x01,         //   Usage Page (Generic Desktop)
    0x09, 0x30,         //   Usage (X)
    0x09, 0x31,         //   Usage (Y)
    0x15, 0x81,         //   Logical Minimum (-127)
    0x25, 0x7F,         //   Logical Maximum (127)
    0x75, 0x08,         //   Report Size (8),
    0x95, 0x02,         //   Report Count (2),
    0x81, 0x06,         //   Input (Data, Variable, Relative)
    0x09, 0x38,         //   Usage (Wheel)
    0x95, 0x01,         //   Report Count (1),
    0x81, 0x06,         //   Input (Data, Variable, Relative)
    0xC0                // End Collection
};


#define CONFIG1_DESC_SIZE        (9+ 9+9+7+ 9+9+7+ 9+9+7)
#define KEYBOARD_HID_DESC_OFFSET (9+9)
#define MEDIA_HID_DESC_OFFSET    (9+9+9+7+9)
#define MOUSE_HID_DESC_OFFSET    (9+9+9+7+9+9+7+9)
static uint8_t const PROGMEM config1_descriptor[CONFIG1_DESC_SIZE] = {
    // configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
    9,                                      // bLength;
    2,                                      // bDescriptorType;
    LSB(CONFIG1_DESC_SIZE),                 // wTotalLength
    MSB(CONFIG1_DESC_SIZE),
    3,                                      // bNumInterfaces
    1,                                      // bConfigurationValue
    0,                                      // iConfiguration
    0xC0,                                   // bmAttributes
    50,                                     // bMaxPower

    // interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
    9,                                      // bLength
    4,                                      // bDescriptorType
    KEYBOARD_INTERFACE,                     // bInterfaceNumber
    0,                                      // bAlternateSetting
    1,                                      // bNumEndpoints
    0x03,                                   // bInterfaceClass (0x03 = HID)
    0x01,                                   // bInterfaceSubClass (0x01 = Boot)
    0x01,                                   // bInterfaceProtocol (0x01 = Keyboard)
    0,                                      // iInterface
    // HID interface descriptor, HID 1.11 spec, section 6.2.1
    9,                                      // bLength
    0x21,                                   // bDescriptorType
    0x11, 0x01,                             // bcdHID
    0,                                      // bCountryCode
    1,                                      // bNumDescriptors
    0x22,                                   // bDescriptorType
    sizeof(keyboard_hid_report_desc),       // wDescriptorLength
    0,
    // endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
    7,                                      // bLength
    5,                                      // bDescriptorType
    KEYBOARD_ENDPOINT | 0x80,               // bEndpointAddress
    0x03,                                   // bmAttributes (0x03=intr)
    KEYBOARD_SIZE, 0,                       // wMaxPacketSize
    1,                                      // bInterval

    // second (media keys) interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
    9,                                      // bLength
    4,                                      // bDescriptorType
    MEDIA_INTERFACE,                        // bInterfaceNumber
    0,                                      // bAlternateSetting
    1,                                      // bNumEndpoints
    0x03,                                   // bInterfaceClass (0x03 = HID)
    0x01,                                   // bInterfaceSubClass (0x01 = Boot)
    0x01,                                   // bInterfaceProtocol (0x01 = Keyboard)
    0,                                      // iInterface
    // HID interface descriptor, HID 1.11 spec, section 6.2.1
    9,                                      // bLength
    0x21,                                   // bDescriptorType
    0x11, 0x01,                             // bcdHID
    0,                                      // bCountryCode
    1,                                      // bNumDescriptors
    0x22,                                   // bDescriptorType
    sizeof(media_hid_report_desc),          // wDescriptorLength
    0,
    // endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
    7,                                      // bLength
    5,                                      // bDescriptorType
    MEDIA_ENDPOINT | 0x80,                  // bEndpointAddress
    0x03,                                   // bmAttributes (0x03=intr)
    MEDIA_SIZE, 0,                          // wMaxPacketSize
    1,                                      // bInterval

    // third (mouse) interface descriptor
    9,                                      // bLength
    4,                                      // bDescriptorType
    MOUSE_INTERFACE,                        // bInterfaceNumber
    0,                                      // bAlternateSetting
    1,                                      // bNumEndpoints
    0x03,                                   // bInterfaceClass (0x03 = HID)
    0x01,                                   // bInterfaceSubClass (0x01 = Boot)
    0x02,                                   // bInterfaceProtocol (0x02 = Mouse)
    0,                                      // iInterface
    // HID interface descriptor
    9,                                      // bLength
    0x21,                                   // bDescriptorType
    0x11, 0x01,                             // bcdHID
    0,                                      // bCountryCode
    1,                                      // bNumDescriptors
    0x22,                                   // bDescriptorType
    sizeof(mouse_hid_report_desc),          // wDescriptorLength
    0,
    // endpoint descriptor
    7,                                      // bLength
    5,                                      // bDescriptorType
    MOUSE_ENDPOINT | 0x80,                  // bEndpointAddress
    0x03,                                   // bmAttributes (0x03=intr)
    MOUSE_SIZE, 0,                          // wMaxPacketSize
    0x01                                    // bInterval
};

// If you're desperate for a little extra code memory, these strings
// can be completely removed if iManufacturer, iProduct, iSerialNumber
// in the device desciptor are changed to zeros.
struct usb_string_descriptor_struct {
    uint8_t bLength;
    uint8_t bDescriptorType;
    int16_t wString[];
};
static struct usb_string_descriptor_struct const PROGMEM string0 = {
    4,
    3,
    {0x0409}
};
static struct usb_string_descriptor_struct const PROGMEM string1 = {
    sizeof(STR_MANUFACTURER),
    3,
    STR_MANUFACTURER
};
static struct usb_string_descriptor_struct const PROGMEM string2 = {
    sizeof(STR_PRODUCT),
    3,
    STR_PRODUCT
};

// This table defines which descriptor data is sent for each specific
// request from the host (in wValue and wIndex).
static struct descriptor_list_struct {
    uint16_t        wValue;
    uint16_t        wIndex;
    const uint8_t   *addr;
    uint8_t         length;
} const PROGMEM descriptor_list[] = {
    {0x0100, 0x0000, device_descriptor, sizeof(device_descriptor)},
    {0x0200, 0x0000, config1_descriptor, sizeof(config1_descriptor)},
    {0x2200, KEYBOARD_INTERFACE, keyboard_hid_report_desc, sizeof(keyboard_hid_report_desc)},
    {0x2100, KEYBOARD_INTERFACE, config1_descriptor+KEYBOARD_HID_DESC_OFFSET, 9},
    {0x2200, MEDIA_INTERFACE, media_hid_report_desc, sizeof(media_hid_report_desc)},
    {0x2101, MEDIA_INTERFACE, config1_descriptor+MEDIA_HID_DESC_OFFSET, 9},
    {0x2200, MOUSE_INTERFACE, mouse_hid_report_desc, sizeof(mouse_hid_report_desc)},
    {0x2102, MOUSE_INTERFACE, config1_descriptor+MOUSE_HID_DESC_OFFSET, 9},
    {0x0300, 0x0000, (const uint8_t *)&string0, 4},
    {0x0301, 0x0409, (const uint8_t *)&string1, sizeof(STR_MANUFACTURER)},
    {0x0302, 0x0409, (const uint8_t *)&string2, sizeof(STR_PRODUCT)}
};
#define NUM_DESC_LIST (sizeof(descriptor_list)/sizeof(struct descriptor_list_struct))


/**************************************************************************
 *
 *  Variables - these are the only non-stack RAM usage
 *
 **************************************************************************/

// zero when we are not configured, non-zero when enumerated
static volatile uint8_t usb_configuration=0;

// which modifier keys are currently pressed
// 1=left ctrl,    2=left shift,   4=left alt,    8=left gui
// 16=right ctrl, 32=right shift, 64=right alt, 128=right gui
volatile uint8_t keyboard_modifier_keys=0;
volatile uint8_t keyboard_modifier_keys_acked=0;

// which keys are currently pressed, up to 6 keys may be down at once
volatile uint8_t keyboard_keys[6] = {0, 0, 0, 0, 0, 0};
volatile uint8_t keyboard_keys_acked[6] = {0, 0, 0, 0, 0, 0};

volatile uint16_t media_keys[4] = {0, 0, 0, 0};
volatile uint8_t mouse_buttons = 0;

// protocol setting from the host.  We use exactly the same report
// either way, so this variable only stores the setting since we
// are required to be able to report which setting is in use.
static uint8_t keyboard_protocol=1;

// the idle configuration, how often we send the report to the
// host (ms * 4) even when it hasn't changed
static uint8_t keyboard_idle_config=125;

// count until idle timeout
static uint8_t keyboard_idle_count=0;

// 1=num lock, 2=caps lock, 4=scroll lock, 8=compose, 16=kana
volatile uint8_t keyboard_leds=0;

static uint8_t media_protocol=1;
static uint8_t media_idle_config=125;
static uint8_t media_idle_count=0;

static uint8_t mouse_protocol=1;
static uint8_t mouse_idle_config=125;
static uint8_t mouse_idle_count=0;


/**************************************************************************
 *
 *  Public Functions - these are the API intended for the user
 *
 **************************************************************************/


// initialize USB
void usb_init(void)
{
    HW_CONFIG();
    USB_FREEZE();   // enable USB
    PLL_CONFIG();                           // config PLL
    while (!(PLLCSR & (1<<PLOCK))) ;    // wait for PLL lock
    USB_CONFIG();                               // start USB clock
    UDCON = 0;                          // enable attach resistor
    usb_configuration = 0;
    UDIEN = (1<<EORSTE)|(1<<SOFE);
    sei();
}

// return 0 if the USB is not configured, or the configuration
// number selected by the HOST
uint8_t usb_configured(void)
{
    return usb_configuration;
}


// perform a single keystroke
int8_t usb_keyboard_press(uint8_t key, uint8_t modifier)
{
    int8_t r;

    keyboard_modifier_keys = modifier;
    keyboard_keys[0] = key;
    r = usb_keyboard_send_now();
    if (r) return r;
    keyboard_modifier_keys = 0;
    keyboard_keys[0] = 0;
    return usb_keyboard_send_now();
}

// perform a single keystroke
int8_t usb_media_press(uint16_t key)
{
    int8_t r;

    media_keys[0] = key;
    r = usb_media_send_now();
    if (r) return r;
    media_keys[0] = 0;
    return usb_media_send_now();
}

static void send_key_data(void);
static void send_media_key_data(void);
static void send_mouse_data(int8_t delta_x, int8_t delta_y);

// send the contents of keyboard_keys and keyboard_modifier_keys
void usb_keyboard_send(void)
{
    UENUM = KEYBOARD_ENDPOINT;
    UEIENX |= (1 << TXINE);
}

void usb_media_send(void)
{
    UENUM = MEDIA_ENDPOINT;
    UEIENX |= (1 << TXINE);
}

int8_t usb_mouse_send(uint8_t buttons, int8_t delta_x, int8_t delta_y)
{
    mouse_buttons = buttons;
    uint8_t intr_state, timeout;

    if (!usb_configuration) return -1;
    intr_state = SREG;
    cli();
    UENUM = MOUSE_ENDPOINT;
    timeout = UDFNUML + 50;
    while (1) {
        // are we ready to transmit?
        if (UEINTX & (1<<RWAL)) break;
        SREG = intr_state;
        // has the USB gone offline?
        if (!usb_configuration) return -1;
        // have we waited too long?
        if (UDFNUML == timeout) return -1;
        // get ready to try checking again
        intr_state = SREG;
        cli();
        UENUM = MOUSE_ENDPOINT;
    }
    send_mouse_data(delta_x, delta_y);
    UEINTX = 0x3A;
    mouse_idle_count = 0;
    SREG = intr_state;
    return 0;
}

 // send the contents of keyboard_keys and keyboard_modifier_keys
 int8_t usb_keyboard_send_now(void)
 {
    uint8_t intr_state, timeout;

    if (!usb_configuration) return -1;
    intr_state = SREG;
    cli();
    UENUM = KEYBOARD_ENDPOINT;
    timeout = UDFNUML + 50;
    while (1) {
        // are we ready to transmit?
        if (UEINTX & (1<<RWAL)) break;
        SREG = intr_state;
        // has the USB gone offline?
        if (!usb_configuration) return -1;
        // have we waited too long?
        if (UDFNUML == timeout) return -1;
        // get ready to try checking again
        intr_state = SREG;
        cli();
        UENUM = KEYBOARD_ENDPOINT;
    }
    send_key_data();
    UEINTX = 0x3A;
    keyboard_idle_count = 0;
    SREG = intr_state;
    return 0;
}

int8_t usb_media_send_now(void)
{
    uint8_t intr_state, timeout;

    if (!usb_configuration) return -1;
    intr_state = SREG;
    cli();
    UENUM = MEDIA_ENDPOINT;
    timeout = UDFNUML + 50;
    while (1) {
        // are we ready to transmit?
        if (UEINTX & (1<<RWAL)) break;
        SREG = intr_state;
        // has the USB gone offline?
        if (!usb_configuration) return -1;
        // have we waited too long?
        if (UDFNUML == timeout) return -1;
        // get ready to try checking again
        intr_state = SREG;
        cli();
        UENUM = MEDIA_ENDPOINT;
    }
    send_media_key_data();
    UEINTX = 0x3A;
    media_idle_count = 0;
    SREG = intr_state;
    return 0;
}



/**************************************************************************
 *
 *  Private Functions - not intended for general user consumption....
 *
 **************************************************************************/

static void send_key_data() {
    int i;
    UEDATX = keyboard_modifier_keys;
    UEDATX = 0;
    for (i=0; i<6; i++) {
        UEDATX = keyboard_keys[i];
    }
}

static void send_media_key_data() {
    int i;
    for(i = 0; i < 4; i++) {
        UEDATX = media_keys[i] & 0xff;
        UEDATX = media_keys[i] >> 8;
    }
}

static void send_mouse_data(int8_t delta_x, int8_t delta_y) {
    UEDATX = mouse_buttons;

    UEDATX = delta_x;
    UEDATX = delta_y;

    UEDATX = 0; // wheel
}


// USB Device Interrupt - handle all device-level events
// the transmit buffer flushing is triggered by the start of frame
//
ISR(USB_GEN_vect)
{
    uint8_t intbits, t, i;
    static uint8_t div4=0;

    intbits = UDINT;
    UDINT = 0;
    if (intbits & (1<<EORSTI)) {
        UENUM = 0;
        UECONX = 1;
        UECFG0X = EP_TYPE_CONTROL;
        UECFG1X = EP_SIZE(ENDPOINT0_SIZE) | EP_SINGLE_BUFFER;
        UEIENX = (1<<RXSTPE);
        usb_configuration = 0;
    }
    if ((intbits & (1<<SOFI)) && usb_configuration) {
        if (keyboard_idle_config && (++div4 & 3) == 0) {
            UENUM = KEYBOARD_ENDPOINT;
            if (UEINTX & (1<<RWAL)) {
                keyboard_idle_count++;
                if (keyboard_idle_count == keyboard_idle_config) {
                    keyboard_idle_count = 0;
                    send_key_data();
                    UEINTX = 0x3A;
                }
            }
        }
        if (media_idle_config && (++div4 & 3) == 0) {
            UENUM = MEDIA_ENDPOINT;
            if (UEINTX & (1<<RWAL)) {
                media_idle_count++;
                if (media_idle_count == media_idle_config) {
                    media_idle_count = 0;
                    send_media_key_data();
                    UEINTX = 0x3A;
                }
            }
        }
        if (mouse_idle_config && (++div4 & 3) == 0) {
            UENUM = MOUSE_ENDPOINT;
            if (UEINTX & (1<<RWAL)) {
                mouse_idle_count++;
                if (mouse_idle_count == mouse_idle_config) {
                    mouse_idle_count = 0;
                    send_mouse_data(0, 0);
                    UEINTX = 0x3A;
                }
            }
        }
    }
}



// Misc functions to wait for ready and send/receive packets
static inline void usb_wait_in_ready(void)
{
    while (!(UEINTX & (1<<TXINI))) ;
}
static inline void usb_send_in(void)
{
    UEINTX = ~(1<<TXINI);
}
static inline void usb_wait_receive_out(void)
{
    while (!(UEINTX & (1<<RXOUTI))) ;
}
static inline void usb_ack_out(void)
{
    UEINTX = ~(1<<RXOUTI);
}



//
// USB Endpoint Interrupt
//

ISR(USB_COM_vect)
{
    // device endpoint interrupt handling

    for (uint8_t epnum = FIRST_ENDPOINT; epnum <= LAST_ENDPOINT; epnum++) {
        UENUM = epnum;

        if ((UEIENX & (1 << TXINE)) && // interrupt enabled
            (UEINTX & (1 << TXINI))) { // interrupt fired

            // clear interrupt and disable
            UEINTX &= ~(1 << TXINI);
            UEIENX &= ~(1 << TXINE);

            switch(epnum) {
            case KEYBOARD_ENDPOINT:
                send_key_data();
                UEINTX &= ~(1 << FIFOCON);

                keyboard_modifier_keys_acked = keyboard_modifier_keys;
                memcpy((void *)keyboard_keys_acked, (void *)keyboard_keys, sizeof(keyboard_keys));

                break;
            case MEDIA_ENDPOINT:
                send_media_key_data();
                UEINTX &= ~(1 << FIFOCON);
                break;

            }

        }
    }

    // endpoint 0 handler

    uint8_t intbits;
    const uint8_t *list;
    const uint8_t *cfg;
    uint8_t i, n, len, en;
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
    uint16_t desc_val;
    const uint8_t *desc_addr;
    uint8_t desc_length;

    UENUM = 0;
    intbits = UEINTX;
    if (intbits & (1<<RXSTPI)) {
        bmRequestType = UEDATX;
        bRequest = UEDATX;
        wValue = UEDATX;
        wValue |= (UEDATX << 8);
        wIndex = UEDATX;
        wIndex |= (UEDATX << 8);
        wLength = UEDATX;
        wLength |= (UEDATX << 8);
        UEINTX = ~((1<<RXSTPI) | (1<<RXOUTI) | (1<<TXINI));
        if (bRequest == GET_DESCRIPTOR) {
            list = (const uint8_t *)descriptor_list;
            for (i=0; ; i++) {
                if (i >= NUM_DESC_LIST) {
                    UECONX = (1<<STALLRQ)|(1<<EPEN);  //stall
                    return;
                }
                desc_val = pgm_read_word(list);
                if (desc_val != wValue) {
                    list += sizeof(struct descriptor_list_struct);
                    continue;
                }
                list += 2;
                desc_val = pgm_read_word(list);
                if (desc_val != wIndex) {
                    list += sizeof(struct descriptor_list_struct)-2;
                    continue;
                }
                list += 2;
                desc_addr = (const uint8_t *)pgm_read_word(list);
                list += 2;
                desc_length = pgm_read_byte(list);
                break;
            }
            len = (wLength < 256) ? wLength : 255;
            if (len > desc_length) len = desc_length;
            do {
                // wait for host ready for IN packet
                do {
                    i = UEINTX;
                } while (!(i & ((1<<TXINI)|(1<<RXOUTI))));
                if (i & (1<<RXOUTI)) return;    // abort
                // send IN packet
                n = len < ENDPOINT0_SIZE ? len : ENDPOINT0_SIZE;
                for (i = n; i; i--) {
                    UEDATX = pgm_read_byte(desc_addr++);
                }
                len -= n;
                usb_send_in();
            } while (len || n == ENDPOINT0_SIZE);
            return;
        }
        if (bRequest == SET_ADDRESS) {
            usb_send_in();
            usb_wait_in_ready();
            UDADDR = wValue | (1<<ADDEN);
            return;
        }
        if (bRequest == SET_CONFIGURATION && bmRequestType == 0) {
            usb_configuration = wValue;
            usb_send_in();
            cfg = endpoint_config_table;
            for (i=1; i<=MAX_ENDPOINT; i++) {
                UENUM = i;
                en = pgm_read_byte(cfg++);
                UECONX = en;
                if (en) {
                    UECFG0X = pgm_read_byte(cfg++);
                    UECFG1X = pgm_read_byte(cfg++);
                }
            }
            UERST = 0x1E;
            UERST = 0;
            return;
        }
        if (bRequest == GET_CONFIGURATION && bmRequestType == 0x80) {
            usb_wait_in_ready();
            UEDATX = usb_configuration;
            usb_send_in();
            return;
        }

        if (bRequest == GET_STATUS) {
            usb_wait_in_ready();
            i = 0;
#ifdef SUPPORT_ENDPOINT_HALT
            if (bmRequestType == 0x82) {
                UENUM = wIndex;
                if (UECONX & (1<<STALLRQ)) i = 1;
                UENUM = 0;
            }
#endif
            UEDATX = i;
            UEDATX = 0;
            usb_send_in();
            return;
        }
#ifdef SUPPORT_ENDPOINT_HALT
        if ((bRequest == CLEAR_FEATURE || bRequest == SET_FEATURE)
            && bmRequestType == 0x02 && wValue == 0) {
            i = wIndex & 0x7F;
            if (i >= 1 && i <= MAX_ENDPOINT) {
                usb_send_in();
                UENUM = i;
                if (bRequest == SET_FEATURE) {
                    UECONX = (1<<STALLRQ)|(1<<EPEN);
                } else {
                    UECONX = (1<<STALLRQC)|(1<<RSTDT)|(1<<EPEN);
                    UERST = (1 << i);
                    UERST = 0;
                }
                return;
            }
        }
#endif
        if (wIndex == KEYBOARD_INTERFACE) {
            if (bmRequestType == 0xA1) {
                if (bRequest == HID_GET_REPORT) {
                    usb_wait_in_ready();
                    send_key_data();
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_IDLE) {
                    usb_wait_in_ready();
                    UEDATX = keyboard_idle_config;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_PROTOCOL) {
                    usb_wait_in_ready();
                    UEDATX = keyboard_protocol;
                    usb_send_in();
                    return;
                }
            }
            if (bmRequestType == 0x21) {
                if (bRequest == HID_SET_REPORT) {
                    usb_wait_receive_out();
                    keyboard_leds = UEDATX;
                    usb_ack_out();
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_IDLE) {
                    keyboard_idle_config = (wValue >> 8);
                    keyboard_idle_count = 0;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_PROTOCOL) {
                    keyboard_protocol = wValue;
                    usb_send_in();
                    return;
                }
            }
        }
        if (wIndex == MEDIA_INTERFACE) {
            if (bmRequestType == 0xA1) {
                if (bRequest == HID_GET_REPORT) {
                    usb_wait_in_ready();
                    send_media_key_data();
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_IDLE) {
                    usb_wait_in_ready();
                    UEDATX = media_idle_config;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_PROTOCOL) {
                    usb_wait_in_ready();
                    UEDATX = media_protocol;
                    usb_send_in();
                    return;
                }
            }
            if (bmRequestType == 0x21) {
                if (bRequest == HID_SET_REPORT) {
                    usb_wait_receive_out();
                    keyboard_leds = UEDATX;
                    usb_ack_out();
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_IDLE) {
                    media_idle_config = (wValue >> 8);
                    media_idle_count = 0;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_PROTOCOL) {
                    media_protocol = wValue;
                    usb_send_in();
                    return;
                }
            }
        }
        if (wIndex == MOUSE_INTERFACE) {
            if (bmRequestType == 0xA1) {
                if (bRequest == HID_GET_REPORT) {
                    usb_wait_in_ready();
                    send_mouse_data(0, 0);
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_IDLE) {
                    usb_wait_in_ready();
                    UEDATX = mouse_idle_config;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_GET_PROTOCOL) {
                    usb_wait_in_ready();
                    UEDATX = mouse_protocol;
                    usb_send_in();
                    return;
                }
            }
            if (bmRequestType == 0x21) {
                if (bRequest == HID_SET_REPORT) {
                    usb_wait_receive_out();
                    usb_ack_out();
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_IDLE) {
                    mouse_idle_config = (wValue >> 8);
                    mouse_idle_count = 0;
                    usb_send_in();
                    return;
                }
                if (bRequest == HID_SET_PROTOCOL) {
                    mouse_protocol = wValue;
                    usb_send_in();
                    return;
                }
            }
        }
    }
    UECONX = (1<<STALLRQ) | (1<<EPEN);      // stall
}


/////////////////////
// Debug helpers

static uint8_t _char_to_key[][2] = { 
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },

    { 0, 0 }, // 8
    { 0, KEY_TAB },
    { 0, KEY_ENTER },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    
    { 0, 0 }, // 16
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    
    { 0, 0 }, // 24
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    { 0, 0 },
    
    { 0, KEY_SPACE }, // 32
    { MODIFIER_KEY_SHIFT, KEY_1 },
    { MODIFIER_KEY_SHIFT, KEY_QUOTE },
    { MODIFIER_KEY_SHIFT, KEY_3 },
    { MODIFIER_KEY_SHIFT, KEY_4 },
    { MODIFIER_KEY_SHIFT, KEY_5 },
    { MODIFIER_KEY_SHIFT, KEY_7 },
    { 0, KEY_QUOTE },
    
    { MODIFIER_KEY_SHIFT, KEY_9 }, // 40
    { MODIFIER_KEY_SHIFT, KEY_0 },
    { MODIFIER_KEY_SHIFT, KEY_8 },
    { MODIFIER_KEY_SHIFT, KEY_EQUAL },
    { 0, KEY_COMMA },
    { 0, KEY_MINUS },
    { 0, KEY_PERIOD },
    { 0, KEY_SLASH },
    
    { 0, KEY_0 }, // 48
    { 0, KEY_1 },
    { 0, KEY_2 },
    { 0, KEY_3 },
    { 0, KEY_4 },
    { 0, KEY_5 },
    { 0, KEY_6 },
    { 0, KEY_7 },

    { 0, KEY_8 }, // 56
    { 0, KEY_9 },
    { MODIFIER_KEY_SHIFT, KEY_SEMICOLON },
    { 0, KEY_SEMICOLON },
    { MODIFIER_KEY_SHIFT, KEY_COMMA },
    { 0, KEY_EQUAL },
    { MODIFIER_KEY_SHIFT, KEY_PERIOD },
    { MODIFIER_KEY_SHIFT, KEY_SLASH },
    
    { MODIFIER_KEY_SHIFT, KEY_2 }, // 64
    { MODIFIER_KEY_SHIFT, KEY_A },
    { MODIFIER_KEY_SHIFT, KEY_B },
    { MODIFIER_KEY_SHIFT, KEY_C },
    { MODIFIER_KEY_SHIFT, KEY_D },
    { MODIFIER_KEY_SHIFT, KEY_E },
    { MODIFIER_KEY_SHIFT, KEY_F },
    { MODIFIER_KEY_SHIFT, KEY_G },
    { MODIFIER_KEY_SHIFT, KEY_H },
    { MODIFIER_KEY_SHIFT, KEY_I },
    { MODIFIER_KEY_SHIFT, KEY_J },
    { MODIFIER_KEY_SHIFT, KEY_K },
    { MODIFIER_KEY_SHIFT, KEY_L },
    { MODIFIER_KEY_SHIFT, KEY_M },
    { MODIFIER_KEY_SHIFT, KEY_N },
    { MODIFIER_KEY_SHIFT, KEY_O },
    { MODIFIER_KEY_SHIFT, KEY_P },
    { MODIFIER_KEY_SHIFT, KEY_Q },
    { MODIFIER_KEY_SHIFT, KEY_R },
    { MODIFIER_KEY_SHIFT, KEY_S },
    { MODIFIER_KEY_SHIFT, KEY_T },
    { MODIFIER_KEY_SHIFT, KEY_U },
    { MODIFIER_KEY_SHIFT, KEY_V },
    { MODIFIER_KEY_SHIFT, KEY_W },
    { MODIFIER_KEY_SHIFT, KEY_X },
    { MODIFIER_KEY_SHIFT, KEY_Y },
    { MODIFIER_KEY_SHIFT, KEY_Z },

    { 0, KEY_LEFT_BRACE }, // 91
    { 0, KEY_BACKSLASH },
    { 0, KEY_RIGHT_BRACE },
    { MODIFIER_KEY_SHIFT, KEY_6 },
    { MODIFIER_KEY_SHIFT, KEY_MINUS },

    { 0, KEY_TILDE }, // 96
    { 0, KEY_A },
    { 0, KEY_B },
    { 0, KEY_C },
    { 0, KEY_D },
    { 0, KEY_E },
    { 0, KEY_F },
    { 0, KEY_G },
    { 0, KEY_H },
    { 0, KEY_I },
    { 0, KEY_J },
    { 0, KEY_K },
    { 0, KEY_L },
    { 0, KEY_M },
    { 0, KEY_N },
    { 0, KEY_O },
    { 0, KEY_P },
    { 0, KEY_Q },
    { 0, KEY_R },
    { 0, KEY_S },
    { 0, KEY_T },
    { 0, KEY_U },
    { 0, KEY_V },
    { 0, KEY_W },
    { 0, KEY_X },
    { 0, KEY_Y },
    { 0, KEY_Z },

    { MODIFIER_KEY_SHIFT, KEY_LEFT_BRACE }, // 123
    { MODIFIER_KEY_SHIFT, KEY_BACKSLASH },
    { MODIFIER_KEY_SHIFT, KEY_RIGHT_BRACE },
    { MODIFIER_KEY_SHIFT, KEY_TILDE },
    { 0, 0 },
};

void usb_keyboard_send_message(char *msg) {
    for(char *ltr = msg; *ltr; ltr++) {

        uint8_t *values = _char_to_key[*ltr & 0x7f];
        if (!values[1]) {
            continue;
        }

        usb_keyboard_press(values[1], values[0]);
    }
}

