#ifndef FIRMWARE_SIM868_H
#define FIRMWARE_SIM868_H

#ifndef SIM868_USART
#  error  You must define SIM868_USART before include "sim868.h"
#endif
#if SIM868_USART == 0
#   ifndef USART0_ENABLE
#       error You choise USART0 to SIM868, but USART0 is disabled
#   endif
#elif SIM868_USART == 1
#   ifndef USART1_ENABLE
#       error You choise USART1 to SIM868, but USART1 is disabled
#   endif
#else
#   error Unknown USART port.
#endif

#define SIM868_BUFFER_LENGTH    255

void sim868_init(void);
void sim868_receive(uint8_t data);

#endif //FIRMWARE_SIM868_H
