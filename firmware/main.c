#include "main.h"
#include "millis.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "obd2.h"
#include <stdio.h>
#include "utils.h"
#include <string.h>
#include "modbus.h"

uint8_t powersave;
ptimer_t main_powerTon;
ptimer_t main_powersaveTof_long;

extern uint32_t _millis;
uint16_t timer2 = 0;
sleepmode_t sleepmode;
// 125 Hz
ISR(TIMER2_COMP_vect) {
    if (SLEEP == sleepmode) {
        timer2++;

        if (timer2 >= 10 * SLEEP_TIMER_FREQ) {
            sleepmode = WAKEUP;
            timer2 = 0;
        }
    } else {
        timer2 = 0;
    }
}

void main_reset(void) {
    _millis = 0;
    obd2_reset();
    powersave = 1;
    pTimerReset(&main_powerTon);
    pTimerReset(&main_powersaveTof_long);
}

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    sim868_pwr_off();
    LED_OFF();

    millis_init();
    obd2_init();
    modbus_init();

    sleepmode = WORK;

    main_reset();
}

void loop(void) {
    modbus_loop();

    if (obd2_loop()) { // change obd code
        uint8_t engine = obd2_engine_working();
        uint8_t p = engine || (ptof(&main_powersaveTof_long, pton(&main_powerTon, engine, 30000), 600000));
        powersave = !p;
    }

    uint32_t _millis = millis();
    uint32_t runtime = obd2_get_runtime_since_engine_start();
    uint32_t distance = obd2_get_aprox_distance_traveled();
    int8_t temperature = obd2_engine_coolant_temperature;
    uint8_t vehicle_kmh = obd2_vehicle_speed;
    uint16_t engine_rpm = obd2_engine_speed;
    uint32_t timestamp = obd2_timestamp;

    modbus_put32(0, 1, _millis);
    modbus_put32(2, 3, runtime);
    modbus_put32(4, 5, distance);
    modbus_put16(6, engine_rpm);
    modbus_put8(7, (uint8_t)temperature, vehicle_kmh);
    modbus_put32(8, 9, timestamp);

#ifdef POWERMODE_OFF
    powersave = 1;
#endif
#ifdef POWERMODE_ON
    powersave = 0;
#endif
}

int main(void) {
	cli();
	init();
	_delay_ms(300);
    blink(1);
    _delay_ms(300);
    blink(1);

	// SLEEP TIMER
    // CTC, prescaler: 1024
    TCCR2A = (0<<WGM20)|(0<<COM2A1)|(0<<COM2A0)|(1<<WGM21)|(1<<CS22)|(1<<CS21)|(1<<CS20);
    TCNT2 = 0x00;
    OCR2A = ((F_CPU / 1024) / SLEEP_TIMER_FREQ) - 1;
    TIMSK2 |= (1<<OCIE2A);
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);

    // WATCHDOG
    wdt_enable(WDTO_2S);
//    if (MCUSR & (1<<WDRF)) {
//        usart_println_sync(main_usart, "SYSTEM: Watchdog reset vector");
//    }
    MCUSR &= ~(1<<WDRF);
	sei();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(1) {
	    switch (sleepmode) {
	        case GOTOSLEEP:
	            long_blink(1);
	            timer2 = 0;
	            wdt_disable();
	            sim868_pwr_off();
	            sleepmode = SLEEP;
	            break;
	        case SLEEP:
	            sleep_mode();
                break;
	        case WAKEUP:
	            long_blink(2);
                main_reset();
                wdt_enable(WDTO_2S);
                sleepmode = WORK;
                break;
	        default:
                wdt_reset();
                loop();
                break;
	    }
	}
#pragma clang diagnostic pop
}
