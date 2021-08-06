#include "main.h"
#include "millis.h"
#include "usart_lib.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "obd2.h"
#include "sim868.h"
#include <stdio.h>
#include "utils.h"

usart_t *usart;
uint8_t usart_rx_buffer;
powermode_t pwrMode;

usart_t* get_main_usart(void) {
    return usart;
}

void usart_rx(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
    usart_send_sync(usart, data); // echo
    usart_send_sync(sim868_get_usart(), data);
#endif
}

void init(void) {
    SETBIT_1(LED_DDR, LED_Pn);
    SETBIT_0(BTN_DDR, BTN_Pn);
    SETBIT_1(SIM868_PWR_DDR, SIM868_PWR_Pn);
    LED_OFF();

    millis_init();

#   if MAIN_USART == 0
    usart = &usart0;
#   elif MAIN_USART == 1
    usart = &usart1;
#   endif
    usart->rx_vec = usart_rx;

    usart_init(usart, 9600);

    obd2_init();
    sim868_init();
    sim868_httpUrl("http://dkotlyar.ru:8000/telemetry");

    usart_println_sync(usart, "Start up firmware. Build 5");
#ifdef SIM868_USART_BRIDGE
    usart_println_sync(usart, "SIM868 Bridge mode");
#endif
    pwrMode = POWER_AUTOMATIC;
    blink(3);
    usart_println_sync(usart, "Power mode: AUTOMATIC");
}

void loop(void) {
    static uint16_t keyPressed = 0;
    if (read_key()) {
        if (keyPressed < 1000) {
            keyPressed++;
        } else if (1000 == keyPressed) {
            switch (pwrMode) {
                case POWER_OFF:
                    pwrMode = POWER_ON;
                    blink(2);
                    usart_println_sync(usart, "Power mode: ON");
                    break;
                case POWER_ON:
                    pwrMode = POWER_AUTOMATIC;
                    blink(3);
                    usart_println_sync(usart, "Power mode: AUTOMATIC");
                    break;
                default:
                    pwrMode = POWER_OFF;
                    blink(1);
                    usart_println_sync(usart, "Power mode: OFF");
                    break;
            }
            keyPressed = ~0;
        }
    } else {
        keyPressed = 0;
    }

#ifndef SIM868_USART_BRIDGE
    uint32_t _millis = millis();
    obd2_loop();
    static ptimer_t pston;
    static ptimer_t telemetryTon;

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

    if (pton(&telemetryTon, 1, 5000)) {
        pTimerReset(&telemetryTon);
        if (sim868_status() == SIM868_STATUS_OK || sim868_status() == SIM868_STATUS_OFF) {
            char temp[12];
            sprintf(temp, "%d", engine_coolant_temperature);
            usart_print_sync(get_main_usart(), "Temperature: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%lu", obd2_get_runtime_since_engine_start());
            usart_print_sync(get_main_usart(), "Run time: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", engine_speed);
            usart_print_sync(get_main_usart(), "Engine speed: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", vehicle_speed);
            usart_print_sync(get_main_usart(), "Vehicle speed: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%lu", obd2_get_aprox_distance_traveled());
            usart_print_sync(get_main_usart(), "Distance aprox: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%lu", timestamp);
            usart_print_sync(get_main_usart(), "Timestamp: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%lu", _millis);
            usart_print_sync(get_main_usart(), "Millis: ");
            usart_println_sync(get_main_usart(), temp);
            if (_millis < 60000) {
                usart_println_sync(get_main_usart(), "Run time less 1 minute");
            }
            usart_println_sync(get_main_usart(), "");
        }

        if (!powersave && obd2_get_runtime_since_engine_start() > 0) {
            char buf[255];
            sprintf(buf, "{\"ticks\":%lu,\"imei\":\"%s\",\"gps\":\"%s\",\"gps_timestamp\":%lu,"
                         "\"obd2_timestamp\":%lu,\"run_time\":%lu,\"distance\":%lu,\"engine_rpm\":%u,\"vehicle_kmh\":%u}",
                    _millis, imei, cgnurc, cgnurc_timestamp,
                    timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled(),
                    engine_speed, vehicle_speed);
            sim868_post_async(buf);
        }
    }
#endif
}

int main(void) {
	cli();
	init();
	sei();
	
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(1) {
	    loop();
	}
#pragma clang diagnostic pop
}
