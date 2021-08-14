#include "main.h"
#include "millis.h"
#include "usart_lib.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "obd2.h"
#include "sim868.h"
#include <stdio.h>
#include "utils.h"
#include <string.h>

void setPowerMode(powermode_t pm);

usart_t * main_usart;
extern usart_t * sim868_usart;
powermode_t pwrMode;

uint8_t powersave;
ptimer_t main_powerTon;
ptimer_t main_powersaveTof_long;
ptimer_t main_telemetryTon;
ptimer_t main_obdTon;
uint8_t main_sim868_work;
extern uint8_t sim868_hasNewFile;

extern uint32_t _millis;
uint16_t timer2 = 0;
sleepmode_t sleepmode;
// 125 Hz
ISR(TIMER2_COMP_vect) {
    timer2++;
    if (SLEEP == sleepmode) {
        _millis += 1000 / SLEEP_TIMER_FREQ;
    }

    if (timer2 >= 10 * SLEEP_TIMER_FREQ) {
        if (SLEEP == sleepmode) {
            sleepmode = WAKEUP;
        }
        timer2 = 0;
    }

//    static uint16_t keyPressed = 0;
//#define READKEY_TIME    300UL // ms
//#define READKEY_CYCLES  (READKEY_TIME * SLEEP_TIMER_FREQ / 1000)
//    if (read_key()) {
//        if (keyPressed < READKEY_CYCLES) {
//            keyPressed++;
//        } else if (READKEY_CYCLES == keyPressed) {
//            switch (pwrMode) {
//                case POWER_OFF:
//                    setPowerMode(POWER_ON);
//                    break;
//                case POWER_ON:
//                    setPowerMode(POWER_AUTOMATIC);
//                    break;
//                default:
//                    setPowerMode(POWER_OFF);
//                    break;
//            }
//            keyPressed = ~0;
//        }
//    } else {
//        keyPressed = 0;
//    }
}

void usart_rx(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
    usart_send_sync(main_usart, data); // echo
    usart_send_sync(sim868_usart, data);
#endif
}

void setPowerMode(powermode_t pm) {
    pwrMode = pm;
    switch (pwrMode) {
        case POWER_ON:
            blink(2);
            usart_println_sync(main_usart, "Power mode: ON");
            break;
        case POWER_AUTOMATIC:
            blink(3);
            usart_println_sync(main_usart, "Power mode: AUTOMATIC");
            break;
        default:
            pwrMode = POWER_OFF;
            blink(1);
            usart_println_sync(main_usart, "Power mode: OFF");
            break;
    }
}

void main_reset(void) {
    _millis = 0;
    obd2_reset();
    sim868_reset();
    powersave = 0;
    pTimerReset(&main_powerTon);
    pTimerReset(&main_powersaveTof_long);
    pTimerReset(&main_telemetryTon);
    pTimerReset(&main_obdTon);
    main_sim868_work = 0;
}

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    sim868_pwr_off();
    LED_OFF();

    millis_init();

#   if MAIN_USART == 0
    main_usart = &usart0;
#   elif MAIN_USART == 1
    main_usart = &usart1;
#   endif
    main_usart->rx_vec = usart_rx;

    usart_init(main_usart, 57600);

    obd2_init();
    sim868_init();

    usart_println_sync(main_usart, "Start up firmware. Build 6");
#ifdef SIM868_USART_BRIDGE
    usart_println_sync(main_usart, "SIM868 Bridge mode");
#endif

    setPowerMode(DEFAULT_POWER_MODE);
    sleepmode = WORK;

    main_reset();
}

void loop(void) {
#ifndef SIM868_USART_BRIDGE
    uint32_t _millis = millis();
    if (obd2_loop()) { // change obd code
        uint8_t engine = obd2_get_runtime_since_engine_start() > 0;
        uint8_t p = engine || (ptof(&main_powersaveTof_long, pton(&main_powerTon, engine, 10000), 60000) && sim868_hasNewFile);
        powersave = !p;
    }

    if (POWER_OFF == pwrMode) {
        powersave = 1;
    } else if (POWER_ON == pwrMode) {
        powersave = 0;
    }

    main_sim868_work = main_sim868_work || !powersave;
    if (main_sim868_work) {
        uint8_t sim868_retval = sim868_loop(powersave);
        if (SIM868_LOOP_RET_POWERDOWN == sim868_retval) {
            main_sim868_work = 0;
        }
    }
    if (!main_sim868_work && powersave) {
        sleepmode = GOTOSLEEP;
    }

#ifdef OBD2_DEBUG
    if (pton(&main_obdTon, 1, 2000)) {
        pTimerReset(&main_obdTon);
        char temp[12];
        usart_print_sync(main_usart, "> +OBD: ");
        obd_log("%lu", _millis); usart_print_sync(main_usart, ",");
        obd_log("%d", obd2_engine_coolant_temperature); usart_print_sync(main_usart, ",");
        obd_log("%lu", obd2_get_runtime_since_engine_start()); usart_print_sync(main_usart, ",");
        obd_log("%u", obd2_engine_speed); usart_print_sync(main_usart, ",");
        obd_log("%u", obd2_vehicle_speed); usart_print_sync(main_usart, ",");
        obd_log("%lu", obd2_get_aprox_distance_traveled()); usart_println_sync(main_usart, "");
    }
#endif

    if (!powersave && pton(&main_telemetryTon, 1, obd2_get_runtime_since_engine_start() > 0 ? 5000 : 60000)) {
        pTimerReset(&main_telemetryTon);
        char buf[SIM868_CHARBUFFER_LENGTH];
        sprintf(buf, "{\"ticks\":%lu,\"sim868_imei\":\"%s\",\"gps\":\"%s\",\"gps_timestamp\":%lu,"
                     "\"obd2_timestamp\":%lu,\"run_time\":%lu,\"distance\":%lu,\"engine_rpm\":%u,\"vehicle_kmh\":%u}",
                _millis, sim868_imei, sim868_cgnurc, sim868_cgnurc_timestamp,
                obd2_timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled(),
                obd2_engine_speed, obd2_vehicle_speed);
        sim868_post_async(buf);
    }
#endif
}

int main(void) {
	cli();
	init();

	// SLEEP TIMER
    // CTC, prescaler: 1024
    TCCR2A = (0<<WGM20)|(0<<COM2A1)|(0<<COM2A0)|(1<<WGM21)|(1<<CS22)|(1<<CS21)|(1<<CS20);
    TCNT2 = 0x00;
    OCR2A = ((F_CPU / 1024) / SLEEP_TIMER_FREQ) - 1;
    TIMSK2 |= (1<<OCIE2A);
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);

    // WATCHDOG
    wdt_enable(WDTO_2S);
    if (MCUSR & (1<<WDRF)) {
        usart_println_sync(main_usart, "SYSTEM: Watchdog reset vector");
    }
    MCUSR &= ~(1<<WDRF);
	sei();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(1) {
	    switch (sleepmode) {
	        case GOTOSLEEP:
	            long_blink(1);
	            usart_println_sync(main_usart, "Enter sleep mode");
                _delay_us(1000);
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
                usart_println_sync(main_usart, "Leave sleep mode");
                main_reset();
                wdt_enable(WDTO_2S);
                sleepmode = WORK;
                break;
	        default:
                loop();
                wdt_reset();
                break;
	    }
	}
#pragma clang diagnostic pop
}
