#include "main.h"
#include "sim868.h"
#include "usart_lib.h"
#include <string.h>
#include <stdio.h>

usart_t *sim868_usart;

#if SIM868_BUFFER_LENGTH <= 0xFF
uint8_t rec_buffer_index = 0;
#else
uint16_t rec_buffer_index = 0;
#endif
uint8_t rec_buffer[SIM868_BUFFER_LENGTH];

uint8_t status = SIM868_STATUS_FREE;

char imei[20] = {0};
char cgnurc[115] = {0};

void sim868_init(void) {
#   if SIM868_USART == 0
    sim868_usart = &usart0;
#   elif SIM868_USART == 1
    sim868_usart = &usart1;
#   endif
    sim868_usart->rx_vec = sim868_receive;
    usart_init(sim868_usart, 9600);
}

void sim868_receive(uint8_t data) {
    if (data == '\n' || data == '\r') {
        rec_buffer[rec_buffer_index] = 0;
        sim868_handle_buffer();
    } else {
        rec_buffer[rec_buffer_index] = data;
        rec_buffer_index++;
        if (rec_buffer_index == SIM868_BUFFER_LENGTH) {
            rec_buffer[SIM868_BUFFER_LENGTH] = 0;
            sim868_handle_buffer();
        }
    }
}

usart_t* sim868_get_usart(void) {
    return sim868_usart;
}

void sim868_handle_buffer(void) {
    if (rec_buffer_index == 0) {
        return;
    }
    switch (status) {
        case SIM868_STATUS_WAITIMEI:
//            usart_print_sync(get_main_usart(), "IMEI RESPONSE: ");
//            usart_println_sync(get_main_usart(), rec_buffer);
            memcpy(imei, rec_buffer, rec_buffer_index);
            status = SIM868_STATUS_FREE;
            break;
        default:
            if (memcmp(rec_buffer, "ERROR", 5) == 0 || memcmp(rec_buffer, "OK", 2) == 0) {
                status = SIM868_STATUS_FREE;
            } else if (memcmp(rec_buffer, "+UGNSINF: ", 10) == 0) {
                memcpy(cgnurc, rec_buffer + 10, rec_buffer_index - 10);
            } else if (memcmp(rec_buffer, "+HTTPACTION: ", 13) == 0) {
                blink();
            } else {
                usart_println_sync(get_main_usart(), rec_buffer);
            }
            status = SIM868_STATUS_FREE;
//            usart_t *tx_usart = get_main_usart();
//            usart_send_sync(tx_usart, rec_buffer_index);
//            for (uint8_t i = 0; i < rec_buffer_index; i++) {
//                usart_send_sync(tx_usart, rec_buffer[i]);
//            }
            break;
    }
//    usart_t *tx_usart = get_main_usart();
//    usart_send_sync(tx_usart, rec_buffer_index);
//    for (uint8_t i = 0; i < rec_buffer_index; i++) {
//        usart_send_sync(tx_usart, rec_buffer[i]);
//    }
    rec_buffer_index = 0;
}

void sim868_getIMEI(void) {
    while (status != SIM868_STATUS_FREE);
    usart_println_sync(get_main_usart(), "AT+GSN");
    usart_println_sync(sim868_usart, "AT+GSN");
    status = SIM868_STATUS_WAITIMEI;
}

void sim868_enableGnss(void) {
//    usart_println_sync(get_main_usart(), "enable gnss wait");
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+CGNSPWR=1");
    usart_println_sync(sim868_usart, "AT+CGNSPWR=1");
    status = SIM868_STATUS_BUSY;

//    usart_println_sync(get_main_usart(), "enable cgnsurc wait");
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+CGNSURC=2");
    usart_println_sync(sim868_usart, "AT+CGNSURC=2");
    status = SIM868_STATUS_BUSY;
}

void sim868_httpUrl(char* url) {
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+HTTPINIT");
    usart_println_sync(sim868_usart, "AT+HTTPINIT");
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    usart_print_sync(get_main_usart(), "AT+HTTPPARA=URL,");
    usart_print_sync(sim868_usart, "AT+HTTPPARA=URL,");
    usart_println_sync(get_main_usart(), url);
    usart_println_sync(sim868_usart, url);
    status = SIM868_STATUS_BUSY;
}

void sim868_post(char *data) {
    uint16_t len = strlen(data);
    char lens[6] = {0};
    sprintf(lens, "%d", len);

    SIM868_wait();
    usart_print_sync(get_main_usart(), "AT+HTTPDATA=");
    usart_print_sync(sim868_usart, "AT+HTTPDATA=");
    usart_print_sync(get_main_usart(), lens);
    usart_print_sync(sim868_usart, lens);
    usart_println_sync(get_main_usart(), ",2000");
    usart_println_sync(sim868_usart, ",2000");
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait DOWNLOAD response
    usart_println_sync(sim868_usart, data);
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait OK response
    usart_println_sync(get_main_usart(), "AT+HTTPACTION=1");
    usart_println_sync(sim868_usart, "AT+HTTPACTION=1");
    status = SIM868_STATUS_BUSY;
}
