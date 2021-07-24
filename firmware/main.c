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
}

void loop(void) {
#ifndef SIM868_USART_BRIDGE
    obd2_loop();
    sim868_loop();

    static uint32_t lastStart = 0;
    uint32_t _millis = millis();
    if ((_millis - lastStart) > 5000) {
        if (sim868_status() == SIM868_STATUS_OK) {
            char temp[5];
            sprintf(temp, "%d", engine_coolant_temperature);
            usart_print_sync(get_main_usart(), "Temperature: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", obd2_get_runtime_since_engine_start());
            usart_print_sync(get_main_usart(), "Run time: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", engine_speed);
            usart_print_sync(get_main_usart(), "Engine speed: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", vehicle_speed);
            usart_print_sync(get_main_usart(), "Vehicle speed: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", distance_traveled_since_codes_cleared);
            usart_print_sync(get_main_usart(), "Distance: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%u", obd2_get_aprox_distance_traveled());
            usart_print_sync(get_main_usart(), "Distance aprox: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%ld", timestamp);
            usart_print_sync(get_main_usart(), "Timestamp: ");
            usart_println_sync(get_main_usart(), temp);
            sprintf(temp, "%ld", _millis);
            usart_print_sync(get_main_usart(), "Millis: ");
            usart_println_sync(get_main_usart(), temp);
            usart_println_sync(get_main_usart(), "");
        }

        lastStart = _millis;

        char buf[255];
        sprintf(buf, "{\"ticks\":%lu,\"imei\":\"%s\",\"gps\":\"%s\",\"obd2_timestamp\":%lu,\"run_time\":%u,\"distance\":%lu}", _millis, imei, cgnurc,
                timestamp, obd2_get_runtime_since_engine_start(), obd2_get_aprox_distance_traveled());
        sim868_post_async(buf);
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
