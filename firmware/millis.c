#include "main.h"
#include "millis.h"

uint32_t _millis = 0;
uint32_t millis(void) {
    return _millis;
}
uint64_t micros(void) {
    return (uint64_t)_millis * 1000 + TCNT0 * 64 / 10;
}

#if defined(__AVR_AT90CAN128__)
void millis_init(void) {
    // режим сброс при совпадении
    // предделитель 64
    TCCR0A = (0<<WGM00)|(0<<COM0A1)|(0<<COM0A0)|(1<<WGM01)|(0<<CS02)|(1<<CS01)|(1<<CS00);
    TCNT0 = 0x00;
    OCR0A = ((F_CPU / 64) / MILLIS_TIMER_FREQ) - 1;
    TIMSK0 |= (1<<OCIE0A);
}
ISR(TIMER0_COMP_vect) {
        _millis++;
}
#elif defined(__AVR_ATmega128__)
void millis_init(void) {
    ASSR = 0<<AS0;
    // режим сброс при совпадении
    // предделитель 64
    TCCR0 = (0<<WGM00)|(0<<COM01)|(0<<COM00)|(1<<WGM01)|(1<<CS02)|(0<<CS01)|(0<<CS00);
    TCNT0 = 0x00;
    OCR0 = ((F_CPU / 64) / MILLIS_TIMER_FREQ) - 1;
    TIMSK |= (1<<OCIE0);
}
ISR(TIMER0_COMP_vect) {
        _millis++;
}
#endif