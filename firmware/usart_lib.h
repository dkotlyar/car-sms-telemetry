#ifndef USART_LIB_H
#define USART_LIB_H

typedef struct {
    uint8_t ucsrb;
    uint8_t ucsrc;
    uint8_t readyToTransmit;

    volatile uint8_t * UCSRA;
    volatile uint8_t * UCSRB;
    volatile uint8_t * UCSRC;
    volatile uint8_t * UBRRH;
    volatile uint8_t * UBRRL;
    volatile uint8_t * UDR;

    void (*rx_vec)(uint8_t);
} usart_t;

void usart_init(usart_t *usart, uint16_t baud);
void usart_send_sync(usart_t *usart, uint8_t data);
void usart_print_sync(usart_t *usart, char * str);
void usart_println_sync(usart_t *usart, char * str);

extern usart_t usart0;
extern usart_t usart1;


#endif
