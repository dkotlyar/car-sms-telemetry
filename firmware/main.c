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
    usart_println_sync(usart, "Start up firmware. Build 5");

    obd2_init();

    sim868_init();
    sim868_httpUrl("http://dkotlyar.ru:8000/post_test");
}

void loop(void) {
#ifndef SIM868_USART_BRIDGE
//    obd2_loop();
//    sim868_loop();
    obd2_request_sync(1, 05);

    static uint32_t lastStart = 0;
    uint32_t _millis = millis();
    if ((_millis - lastStart) > 5000) {
        uint8_t status = sim868_status();
//        char status_str[3];
//        sprintf(status_str, "%u", status);
//        usart_print_sync(usart, "IMEI: ");
//        usart_println_sync(usart, imei);
//        usart_print_sync(usart, "GNSS: ");
//        usart_println_sync(usart, cgnurc);
//        usart_print_sync(usart, "Status: ");
//        usart_println_sync(usart, status_str);
        lastStart = _millis;

        char buf[255];
        sprintf(buf, "{\"imei\":\"%s\",\"gps\":\"%s\"}", imei, cgnurc);
        sim868_post_async(buf);
//        sim868_post("http://dkotlyar.ru:8000/post_test", buf);
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
