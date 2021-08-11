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

#define SIM868_DEBUG

#define SIM868_BUFFER_BANKS     4

#define SIM868_STATUS_OK                200
#define SIM868_STATUS_ERROR             1
#define SIM868_STATUS_WAIT_IMEI         2
#define SIM868_STATUS_WAIT_FILENAME     3
#define SIM868_STATUS_WAIT_FILECONTENT  4
#define SIM868_STATUS_WAIT_FILEINPUT    5
#define SIM868_STATUS_WAIT_BUFFERINDEX  6
#define SIM868_STATUS_IGNORE            253
#define SIM868_STATUS_OFF               254
#define SIM868_STATUS_BUSY              255

#define SIM868_LOOP_INIT            0
#define SIM868_LOOP_AWAIT           1
#define SIM868_LOOP_POWER_DOWN      2
#define SIM868_LOOP_CPIN_READY      3
#define SIM868_LOOP_OK              4
#define SIM868_LOOP_GET_IMEI        5
#define SIM868_LOOP_ENABLE_GNSS_1   6
#define SIM868_LOOP_ENABLE_GNSS_2   7
#define SIM868_LOOP_POWERSAVE       8
#define SIM868_LOOP_DISABLE_GNSS_1  9
#define SIM868_LOOP_DISABLE_GNSS_2  10
#define SIM868_LOOP_DISABLE_POWER   11
#define SIM868_LOOP_MKBUFFER        12
#define SIM868_LOOP_READBUFFERINDEX 13

#define SIM868_HTTP_UNDEFINED       0
#define SIM868_HTTP_READY           1
#define SIM868_HTTP_PENDING         2
#define SIM868_HTTP_200             3 // http 200
#define SIM868_HTTP_NETWORK_ERROR   4 // коды Http 60x - ошибка сети
#define SIM868_HTTP_FAILED          5 // любые http коды отличные от 200

#define SIM868_BUFFER_LOCK          (1<<0)
#define SIM868_HTTP_LOCK            (1<<1)

extern char imei[20];
extern char cgnurc[115];
extern uint32_t cgnurc_timestamp;

void sim868_init(void);
void sim868_receive(uint8_t data);
void sim868_handle_buffer(void);
void sim868_loop(uint8_t powersave);
void sim868_post_async(const char *data);

#define SIM868_async_wait() {if (sim868_status != SIM868_STATUS_OK && sim868_status != SIM868_STATUS_ERROR) {break;}}
#define SIM868_busy() { sim868_status = SIM868_STATUS_BUSY; }

#ifdef SIM868_DEBUG
#define sim868_debug(data) { usart_print_sync(main_usart, data); }
#define sim868_debugln(data) { usart_println_sync(main_usart, data); }
#else
#define sim868_debug(data) {}
#define sim868_debugln(data) {}
#endif

#endif //FIRMWARE_SIM868_H
