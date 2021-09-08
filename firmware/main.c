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
uint8_t nanopi_worked;
ptimer_t main_powerTon;
ptimer_t main_powersaveTof_long;
ptimer_t sleepTon;

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
    modbus_clear_reg();
    powersave = 1;
    nanopi_worked = 0;
    pTimerReset(&main_powerTon);
    pTimerReset(&main_powersaveTof_long);
    pTimerReset(&sleepTon);
}

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    sim868_pwr_off();
//    sim868_pwr_on();
    LED_OFF();

    millis_init();
    obd2_init();

    sleepmode = WORK;

    main_reset();
}

void loop(void) {
    modbus_loop();

    uint16_t sleep_delay = modbus_get16(0);
    if (pton(&sleepTon, sleep_delay > 0, sleep_delay * 1000)) {
        nanopi_worked = 0;
        sleepmode = GOTOSLEEP;
    }

    if (obd2_loop()) { // change obd code
        uint8_t engine = obd2_engine_working();
        powersave = !engine;
        if (powersave && !nanopi_worked) {
            sleepmode = GOTOSLEEP;
        }
    }

    if (read_key()) {
        powersave = 0;
    }

    if (!powersave) {
        sim868_pwr_on();
        modbus_start();
        nanopi_worked = 1;
    }

    uint16_t status = 0;
    SETBIT(status, 0, powersave);

    modbus_put32(0, 1, millis());
    modbus_put32(2, 3, obd2_get_runtime_since_engine_start());
    modbus_put32(4, 5, obd2_get_aprox_distance_traveled());
    modbus_put16(6, obd2_engine_speed);
    modbus_put8(7, (uint8_t)obd2_engine_coolant_temperature, obd2_vehicle_speed);
    modbus_put32(8, 9, obd2_timestamp);
    modbus_put16(10, status);

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
	            modbus_stop();
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
