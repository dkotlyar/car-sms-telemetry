#ifndef MAIN_H
#define MAIN_H

// COMMON SECTION

#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#define SETBIT(reg, bit, value)     {if (value) {SETBIT_1(reg, bit)} else {SETBIT_0(reg, bit)}}
#define SETBIT_1(reg, bit)          {reg |= (1<<(bit));}
#define SETBIT_0(reg, bit)          {reg &= ~(1<<(bit));}
#define GETBIT(reg, bit)            ((reg & (1<<bit))>0)

// PROJECT SECTION

typedef enum {
    WORK,
    GOTOSLEEP,
    SLEEP,
    WAKEUP
} sleepmode_t;

#define CAN_BAUDRATE   500        // in kBit

#define SLEEP_TIMER_FREQ    125 // Hz
//#define POWERMODE_OFF
#define POWERMODE_ON

#define LED_DDR	    DDRE
#define LED_PORT	PORTE
#define LED_Pn		4
#define LED_ON()    {SETBIT_0(LED_PORT, LED_Pn);}
#define LED_OFF()   {SETBIT_1(LED_PORT, LED_Pn);}
#define _blink()    {LED_ON();_delay_ms(50);LED_OFF();_delay_ms(50);}
#define _long_blink(){LED_ON();_delay_ms(400);LED_OFF();_delay_ms(250);}
#define blink(n)    {for (int i=0;i<(n);i++){_blink();}}
#define long_blink(n){for (int i=0;i<(n);i++){_long_blink();}}

#define BTN_DDR     DDRE
#define BTN_PIN     PINE
#define BTN_Pn      5
#define read_key()  (!(BTN_PIN & (1<<BTN_Pn)))

#define SIM868_PWR_DDR  DDRA
#define SIM868_PWR_PORT PORTA
#define SIM868_PWR_Pn   0
#define sim868_pwr_on() {SETBIT_0(SIM868_PWR_PORT, SIM868_PWR_Pn);}
#define sim868_pwr_off(){SETBIT_1(SIM868_PWR_PORT, SIM868_PWR_Pn);}

#define modbus_get16(i) (holdingRegisters[i])
#define modbus_put8(i, valueH, valueL) {inputRegisters[i] = ((valueH)<<8) | (valueL);}
#define modbus_put16(i, value) {inputRegisters[i] = (value);}
#define modbus_put32(i, j, value) { \
    inputRegisters[i] = (value) & 0xFFFF;\
    inputRegisters[j] = ((value) >> 16) & 0xFFFF;}

#endif