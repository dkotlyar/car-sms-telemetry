#ifndef MAIN_H
#define MAIN_H

// COMMON SECTION

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "usart_lib.h"

uint32_t get_millis(void);
usart_t* get_main_usart(void);

#define SETBIT(reg, bit, value)     {if (value) {SETBIT_1(reg, bit)} else {SETBIT_0(reg, bit)}}
#define SETBIT_1(reg, bit)          {reg |= (1<<(bit));}
#define SETBIT_0(reg, bit)          {reg &= ~(1<<(bit));}
#define GETBIT(reg, bit)            ((reg & (1<<bit))>0)
#define MAX(a, b)                   ( (a)>(b) ? (a) : (b) )
#define MIN(a, b)                   ( (a)<(b) ? (a) : (b) )

// PROJECT SECTION

#define USART0_ENABLE
#define USART1_ENABLE
#define CAN_ENABLE

#define MAIN_USART      1   // Specified USART port for main communication
#define SIM868_USART    0   // Specified USART port for SIM868 module

#define CAN_BAUDRATE   500        // in kBit

#define LED_DDR	    DDRE
#define LED_PORT	PORTE
#define LED_Pn		4
#define LED_ON()    {SETBIT_0(LED_PORT, LED_Pn);}
#define LED_OFF()   {SETBIT_1(LED_PORT, LED_Pn);}
#define blink()     {LED_ON();_delay_ms(50);LED_OFF();_delay_ms(50);}
#define long_blink(){LED_ON();_delay_ms(400);LED_OFF();_delay_ms(250);}

#endif