#include "main.h"
#include "millis.h"
#include "usart_lib.h"
// Library from example "CAN ATmega32M1" http://www.hpinfotech.ro/cvavr-examples.html | https://forum.digikey.com/t/can-example-atmega32m1-stk600/13039
// but original Atmel Library does not accessible by link http://www.atmel.com/tools/cansoftwarelibrary.aspx
#include "obd2.h"
#include "utils.h"
#include <stdio.h>

usart_t * main_usart;

void usart_rx(uint8_t data) {
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

    usart_init(main_usart, 9600);

    obd2_init();

    usart_println_sync(main_usart, "Start up firmware. DEBUG OBD v1");
}

void loop(void) {
    static ptimer_t obdTon;
    obd2_loop();

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
