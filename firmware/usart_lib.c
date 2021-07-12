#include "main.h"
#include "usart_lib.h"

#ifdef USART0_ENABLE
usart_t usart0 = {
        .readyToTransmit = (1 << UDRE0),
        .ucsrb = (1 << RXCIE0) | // Разрешить прерывание по приёму данных
                 (1 << RXEN0) | // Разрешить приём данных по USART
                 (1 << TXEN0) | // Разрешить отправку данных по USART
                 (0 << UCSZ02),
        .ucsrc = (0 << UPM01) | (0 << UPM00) |
                 (0 << USBS0) |
                 (1 << UCSZ01) | (1 << UCSZ00),
        .UCSRA = &UCSR0A,
        .UCSRB = &UCSR0B,
        .UCSRC = &UCSR0C,
        .UBRRH = &UBRR0H,
        .UBRRL = &UBRR0L,
        .UDR = &UDR0
};
ISR (USART0_RX_vect) {
        uint8_t data = UDR0;
        (*usart0.rx_vec)(data);
}
#endif

#ifdef USART1_ENABLE
usart_t usart1 = {
        .readyToTransmit = (1 << UDRE1),
        .ucsrb = (1 << RXCIE1) | // Разрешить прерывание по приёму данных
                 (1 << RXEN1) | // Разрешить приём данных по USART
                 (1 << TXEN1) | // Разрешить отправку данных по USART
                 (0 << UCSZ12),
        .ucsrc = (0 << UPM11) | (0 << UPM10) |
                 (0 << USBS1) |
                 (1 << UCSZ11) | (1 << UCSZ10),
        .UCSRA = &UCSR1A,
        .UCSRB = &UCSR1B,
        .UCSRC = &UCSR1C,
        .UBRRH = &UBRR1H,
        .UBRRL = &UBRR1L,
        .UDR = &UDR1
};
ISR (USART1_RX_vect) {
        uint8_t data = UDR1;
        (*usart1.rx_vec)(data);
}
#endif

void usart_init(usart_t *usart, uint16_t baud) {
    *usart->UCSRB = usart->ucsrb;
    *usart->UCSRC = usart->ucsrc;
    uint16_t speed = (F_CPU / 16) / baud - 1;
    *usart->UBRRH = (speed >> 8) & 0xFF;
    *usart->UBRRL = speed & 0xFF;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "LoopDoesntUseConditionVariableInspection"
void usart_send_sync(usart_t *usart, uint8_t data) {
    // ожидаем доступности регистра для записи
    while (!(*(usart->UCSRA) & usart->readyToTransmit));
    *usart->UDR = data; // записываем данные в регистр
}
#pragma clang diagnostic pop

void usart_print_sync(usart_t *usart, char *str) {
    while (*str != 0) {
        usart_send_sync(usart, *str);
        str++;
    }
}

void usart_println_sync(usart_t *usart, char *str) {
    usart_print_sync(usart, str);
    usart_send_sync(usart, '\r');
    usart_send_sync(usart, '\n');
}