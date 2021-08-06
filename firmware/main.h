#ifndef MAIN_H
#define MAIN_H

// COMMON SECTION

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include "usart_lib.h"

#define SETBIT(reg, bit, value)     {if (value) {SETBIT_1(reg, bit)} else {SETBIT_0(reg, bit)}}
#define SETBIT_1(reg, bit)          {reg |= (1<<(bit));}
#define SETBIT_0(reg, bit)          {reg &= ~(1<<(bit));}
#define GETBIT(reg, bit)            ((reg & (1<<bit))>0)

// PROJECT SECTION

#define USART0_ENABLE
#define USART1_ENABLE
#define CAN_ENABLE

#define MAIN_USART      1   // Specified USART port for main communication
#define SIM868_USART    0   // Specified USART port for SIM868 module
//#define SIM868_USART_BRIDGE

#define CAN_BAUDRATE   500        // in kBit

#define POWERSAVE

#define LED_DDR	    DDRE
#define LED_PORT	PORTE
#define LED_Pn		4
#define LED_ON()    {SETBIT_0(LED_PORT, LED_Pn);}
#define LED_OFF()   {SETBIT_1(LED_PORT, LED_Pn);}
#define _blink()    {LED_ON();_delay_ms(50);LED_OFF();_delay_ms(50);}
#define long_blink(){LED_ON();_delay_ms(400);LED_OFF();_delay_ms(250);}
#define blink(n)    {for (int i=0;i<n;i++){_blink();}}

#define BTN_DDR     DDRE
#define BTN_PIN     PINE
#define BTN_Pn      5
#define read_key()  (!(BTN_PIN & (1<<BTN_Pn)))

#define SIM868_PWR_DDR  DDRA
#define SIM868_PWR_PORT PORTA
#define SIM868_PWR_Pn   0
#define sim868_pwr_on() {SETBIT_0(SIM868_PWR_PORT, SIM868_PWR_Pn);}
#define sim868_pwr_off(){SETBIT_1(SIM868_PWR_PORT, SIM868_PWR_Pn);}

typedef enum {
    POWER_OFF,
    POWER_ON,
    POWER_AUTOMATIC
} powermode_t;

#endif