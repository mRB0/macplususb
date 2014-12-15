#ifndef KBCOMM_H_
#define KBCOMM_H_

#include <stdint.h>

#define KB_CLK_PORT PORTE
#define KB_CLK_PIN PINE
#define KB_CLK_DDR DDRE
#define KB_CLK_BIT 7

#define KB_DATA_PORT PORTB
#define KB_DATA_PIN PINB
#define KB_DATA_DDR DDRB
#define KB_DATA_BIT 0

void kb_setup(void);
void kb_readbyte(void (*read_completed)(uint8_t result, uint8_t data));
void kb_writebyte(uint8_t data, void (*write_completed)(uint8_t result));
void kb_postisr(void);
uint8_t kb_isr_fired(void);

#endif
