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

#define SIM868_STATUS_FREE      0
#define SIM868_STATUS_WAITIMEI  1
#define SIM868_STATUS_BUSY      255

extern char imei[20];
extern char cgnurc[115];

void sim868_init(void);
void sim868_receive(uint8_t data);
usart_t* sim868_get_usart(void);
void sim868_handle_buffer(void);
void sim868_getIMEI(void);
void sim868_enableGnss(void);
void sim868_httpUrl(char* url);
void sim868_post(char *data);

#define SIM868_wait() {while (status != SIM868_STATUS_FREE) {_delay_ms(1);}}

#endif //FIRMWARE_SIM868_H
