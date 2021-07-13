#ifndef FIRMWARE_MILLIS_H
#define FIRMWARE_MILLIS_H

#define MILLIS_TIMER_FREQ 1000

uint32_t millis(void);
uint64_t micros(void);
void millis_init(void);

#endif //FIRMWARE_MILLIS_H
