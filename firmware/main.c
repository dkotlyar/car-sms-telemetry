#include "main.h"
#include "millis.h"
#include "usart_lib.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "obd2.h"
#include "sim868.h"
#include <stdio.h>

usart_t *usart;
uint8_t usart_rx_buffer;

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

    usart_println_sync(usart, "Start up firmware. Build 5");
#ifdef SIM868_USART_BRIDGE
    usart_println_sync(usart, "SIM868 Bridge mode");
#endif
}

void loop(void) {
    static uint16_t keyPressed = 0;
    static uint8_t pwrState = 1;
    if (read_key()) {
        if (keyPressed < 1000) {
            keyPressed++;
        } else if (1000 == keyPressed) {
            pwrState = !pwrState;
            keyPressed = ~0;
        }
    } else {
        keyPressed = 0;
    }

    if (pwrState) {
        LED_ON();
        SETBIT_0(SIM868_PWR_PORT, SIM868_PWR_Pn);
    } else {
        LED_OFF();
        SETBIT_1(SIM868_PWR_PORT, SIM868_PWR_Pn);
    }

#ifndef SIM868_USART_BRIDGE
    uint32_t _millis = millis();
    obd2_loop();

#ifdef POWERSAVE
    static uint32_t lastEngineWorkTime = 0;
    if (obd2_get_runtime_since_engine_start() > 0) {
        lastEngineWorkTime = _millis;
    }
    uint8_t powersave = 0;
    if ((_millis - lastEngineWorkTime) > 30000) {
        powersave = 1;
    }
    sim868_loop(powersave);
#else
    sim868_loop(0);
#endif

    static uint32_t lastStart = 0;
    if ((_millis - lastStart) > 5000) {
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

        lastStart = _millis;

#ifndef POWERSAVE
        if (obd2_get_runtime_since_engine_start() > 0) {
#endif
        char buf[255];
        sprintf(buf, "{\"ticks\":%lu,\"imei\":\"%s\",\"gps\":\"%s\",\"gps_timestamp\":%lu,"
                     "\"obd2_timestamp\":%lu,\"run_time\":%lu,\"distance\":%lu,\"engine_rpm\":%u,\"vehicle_kmh\":%u}",
                _millis, imei, cgnurc, cgnurc_timestamp,
                timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled(),
                engine_speed, vehicle_speed);
        sim868_post_async(buf);
#ifndef POWERSAVE
        }
#endif
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
