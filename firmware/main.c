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

usart_t * main_usart;
uint8_t usart_rx_buffer;
extern usart_t * sim868_usart;
powermode_t pwrMode;

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

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    LED_OFF();

    millis_init();

#   if MAIN_USART == 0
    main_usart = &usart0;
#   elif MAIN_USART == 1
    main_usart = &usart1;
#   endif
    main_usart->rx_vec = usart_rx;

    usart_init(main_usart, 9600);

    obd2_init();
    sim868_init();

    usart_println_sync(main_usart, "Start up firmware. Build 6");
#ifdef SIM868_USART_BRIDGE
    usart_println_sync(main_usart, "SIM868 Bridge mode");
#endif

    setPowerMode(POWER_ON);
}

void loop(void) {
#ifndef SIM868_USART_BRIDGE
    static uint16_t keyPressed = 0;
    if (read_key()) {
        if (keyPressed < 1000) {
            keyPressed++;
        } else if (1000 == keyPressed) {
            switch (pwrMode) {
                case POWER_OFF:
                    setPowerMode(POWER_ON);
                    break;
                case POWER_ON:
                    setPowerMode(POWER_AUTOMATIC);
                    break;
                default:
                    setPowerMode(POWER_OFF);
                    break;
            }
            keyPressed = ~0;
        }
    } else {
        keyPressed = 0;
    }

    uint32_t _millis = millis();
    obd2_loop();
    static ptimer_t pston;
    static ptimer_t telemetryTon;
    static ptimer_t obdTon;

    uint8_t powersave = 0;

    if (pton(&pston, 0 == obd2_get_runtime_since_engine_start(), 30000)) {
        powersave = 1;
    }

    if (POWER_OFF == pwrMode) {
        powersave = 1;
    } else if (POWER_ON == pwrMode) {
        powersave = 0;
    }

    sim868_loop(powersave);

    if (pton(&obdTon, 1, 2000)) {
        pTimerReset(&obdTon);
        char temp[12];
        usart_print_sync(main_usart, "> +OBD: ");
        obd_log("%lu", millis()); usart_print_sync(main_usart, ",");
        obd_log("%d", engine_coolant_temperature); usart_print_sync(main_usart, ",");
        obd_log("%lu", obd2_get_runtime_since_engine_start()); usart_print_sync(main_usart, ",");
        obd_log("%u", engine_speed); usart_print_sync(main_usart, ",");
        obd_log("%u", vehicle_speed); usart_print_sync(main_usart, ",");
        obd_log("%lu", obd2_get_aprox_distance_traveled()); usart_println_sync(main_usart, "");
    }

    if (!powersave && pton(&telemetryTon, 1, obd2_get_runtime_since_engine_start() > 0 ? 5000 : 60000)) {
        pTimerReset(&telemetryTon);
        char buf[SIM868_CHARBUFFER_LENGTH];
        sprintf(buf, "{\"ticks\":%lu,\"imei\":\"%s\",\"gps\":\"%s\",\"gps_timestamp\":%lu,"
                     "\"obd2_timestamp\":%lu,\"run_time\":%lu,\"distance\":%lu,\"engine_rpm\":%u,\"vehicle_kmh\":%u}",
                _millis, imei, cgnurc, cgnurc_timestamp,
                timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled(),
                engine_speed, vehicle_speed);
        sim868_post_async(buf);
    }
#endif
}

int main(void) {
	cli();
	init();
    wdt_enable(WDTO_1S);
    if (MCUSR & (1<<WDRF)) {
        usart_println_sync(main_usart, "SYSTEM: Watchdog reset vector");
    }
    MCUSR &= ~(1<<WDRF);
	sei();

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(1) {
	    loop();
	    wdt_reset();
	}
#pragma clang diagnostic pop
}
