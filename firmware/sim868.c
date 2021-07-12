#include "main.h"
#include "sim868.h"
#include "usart_lib.h"

usart_t *usart;

#if SIM868_BUFFER_LENGTH <= 0xFF
uint8_t rec_buffer_index = 0;
#else
uint16_t rec_buffer_index = 0;
#endif
uint8_t rec_buffer[SIM868_BUFFER_LENGTH];

void sim868_init(void) {
#   if SIM868_USART == 0
    usart = &usart0;
#   elif SIM868_USART == 1
    usart = &usart1;
#   endif
    usart->rx_vec = sim868_receive;
}

void sim868_receive(uint8_t data) {
    if (data == '\n') {
        rec_buffer[rec_buffer_index] = 0;
        rec_buffer_index = 0;
        sim868_handle_buffer();
    } else {
        rec_buffer[rec_buffer_index] = data;
        rec_buffer_index++;
        if (rec_buffer_index == SIM868_BUFFER_LENGTH) {
            rec_buffer[SIM868_BUFFER_LENGTH] = 0;
            rec_buffer_index = 0;
            sim868_handle_buffer();
        }
    }
}

void sim868_handle_buffer(void) {

}

