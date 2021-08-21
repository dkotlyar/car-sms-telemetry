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

//usart_t * main_usart;
//extern usart_t * sim868_usart;

uint8_t powersave;
ptimer_t main_powerTon;
ptimer_t main_powersaveTof_long;
ptimer_t main_telemetryTon;
ptimer_t main_obdTon;
uint8_t main_sim868_work;
extern uint8_t sim868_hasNewFile;
extern char sim868_imei[SIM868_IMEI_SIZE];
extern char sim868_cgnurc[SIM868_CGNURC_SIZE];
extern char sim868_gps_session[SIM868_GPS_SESSION_SIZE];
extern uint32_t sim868_cgnurc_timestamp;
uint8_t main_obd2_loop_passed;

extern uint32_t _millis;
uint16_t timer2 = 0;
sleepmode_t sleepmode;
// 125 Hz
ISR(TIMER2_COMP_vect) {
    if (SLEEP == sleepmode) {
        timer2++;
        //    if (SLEEP == sleepmode) {
        //        _millis += 1000 / SLEEP_TIMER_FREQ;
        //    }

        if (timer2 >= 10 * SLEEP_TIMER_FREQ) {
            sleepmode = WAKEUP;
            timer2 = 0;
        }
    } else {
        timer2 = 0;
    }
}

void usart_rx(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
    usart_send_sync(main_usart, data); // echo
    usart_send_sync(sim868_usart, data);
#endif
}

void main_reset(void) {
    _millis = 0;
    obd2_reset();
    sim868_reset();
    powersave = 1;
    pTimerReset(&main_powerTon);
    pTimerReset(&main_powersaveTof_long);
    pTimerReset(&main_telemetryTon);
    pTimerReset(&main_obdTon);
    main_sim868_work = 0;
    main_obd2_loop_passed = 0;
}

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    sim868_pwr_off();
    LED_OFF();

    millis_init();

    main_usart->rx_vec = usart_rx;

    usart_init(main_usart, 57600);

    obd2_init();
    sim868_init();

    usart_println_sync(main_usart, "Start up firmware. Build 7");
#ifdef SIM868_USART_BRIDGE
    usart_println_sync(main_usart, "SIM868 Bridge mode");
#endif

    sleepmode = WORK;

    main_reset();
}

void loop(void) {
#ifndef SIM868_USART_BRIDGE
    uint32_t _millis = millis();
    if (obd2_loop()) { // change obd code
        main_obd2_loop_passed = 1;
        uint8_t engine = obd2_engine_working();
        uint8_t p = engine || (ptof(&main_powersaveTof_long, pton(&main_powerTon, engine, 30000), 600000) && sim868_hasNewFile);
        powersave = !p;
    }

#ifdef POWERMODE_OFF
    powersave = 1;
#endif
#ifdef POWERMODE_ON
    powersave = 0;
#endif

    main_sim868_work = main_sim868_work || !powersave;
    if (main_sim868_work) {
        uint8_t sim868_retval = sim868_loop(powersave);
        if (SIM868_LOOP_RET_POWERDOWN == sim868_retval) {
            main_sim868_work = 0;
        }
    }
    if (!main_sim868_work && powersave && main_obd2_loop_passed) {
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

//    blink(1);
    if (main_obd2_loop_passed && !powersave && pton(&main_telemetryTon, obd2_engine_working(), 5000)) {
//        long_blink(1);
        pTimerReset(&main_telemetryTon);
        char buf[SIM868_CHARBUFFER_LENGTH];
        memset(buf, 0, SIM868_CHARBUFFER_LENGTH);
        sprintf(buf, "{\"ticks\":%lu,\"imei\":\"%s\",\"gps\":\"%s\",\"gps_timestamp\":%lu,\"session\":\"%s\","
                     "\"obd2_timestamp\":%lu,\"run_time\":%lu,\"distance\":%lu,\"engine_rpm\":%u,\"vehicle_kmh\":%u}",
                     _millis, sim868_imei, sim868_cgnurc, sim868_cgnurc_timestamp, sim868_gps_session,
                obd2_timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled(),
                obd2_engine_speed, obd2_vehicle_speed);
        sim868_post_async(buf);
        memset(sim868_cgnurc, 0, SIM868_CGNURC_SIZE);
        sim868_cgnurc_timestamp = 0;
    }
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
//                wdt_enable(WDTO_2S);
                sleepmode = WORK;
                break;
	        default:
//                wdr();
                wdt_reset();
                loop();
                break;
	    }
	}
#pragma clang diagnostic pop
}
