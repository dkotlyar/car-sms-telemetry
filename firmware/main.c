#include "main.h"
#include "usart_lib.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "canlib/can_lib.h"
#include "obd2.h"
#include "sim868.h"
#include <stdio.h>

usart_t *usart;
uint8_t usart_rx_buffer;

uint32_t _millis = 0;
uint32_t get_millis(void) {
    return _millis;
}

usart_t* get_main_usart(void) {
    return usart;
}

void usart_rx(uint8_t data) {
//    long_blink();
//    usart_send_sync(usart, data);
//    usart_send_sync(sim868_get_usart(), data);
}

void init(void) {
#   if MAIN_USART == 0
    usart = &usart0;
#   elif MAIN_USART == 1
    usart = &usart1;
#   endif
    usart->rx_vec = usart_rx;

    usart_init(usart, 9600);
    usart_println_sync(usart, "Start up firmware. Build 4");
//#   ifdef CAN_ENABLE
//    can_init(0);
//    obd2_init();
//#   endif

    sim868_init();
    sim868_init();

    SETBIT_1(LED_DDR, LED_Pn);
    LED_OFF();
}

void loop(void) {
//    sim868_test();
//    usart_send_sync(usart, 'H');
//#   ifdef CAN_ENABLE
//    obd2_loop();
//#   endif
    _delay_ms(5000);
    usart_print_sync(usart, "IMEI: ");
    usart_println_sync(usart, imei);
    usart_print_sync(usart, "GNSS: ");
    usart_println_sync(usart, cgnurc);

    char buf[500];
    sprintf(buf, "{\"imei\":\"%s\",\"gps\":\"%s\"}", imei, cgnurc);
    sim868_post(buf);
}

int main(void) {
	cli();
	init();
	sei();

    sim868_getIMEI();
    sim868_enableGnss();
    sim868_httpUrl("http://dkotlyar.ru:8000/post_test");
	
#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"
	while(1) {
	    loop();
	}
#pragma clang diagnostic pop
}
