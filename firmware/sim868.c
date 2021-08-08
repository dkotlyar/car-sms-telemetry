#include "main.h"
#include "sim868.h"
#include "usart_lib.h"
#include <string.h>
#include <stdio.h>
#include "millis.h"

usart_t *sim868_usart;
extern usart_t * main_usart;

#if SIM868_BUFFER_LENGTH <= 0xFF
uint8_t rec_buffer_index = 0;
#else
uint16_t rec_buffer_index = 0;
#endif
uint8_t rec_buffer_bank_write = 0;
uint8_t rec_buffer_bank_read = 0;
uint8_t rec_buffer_ignore_message = 0;
uint8_t rec_buffer[SIM868_BUFFER_BANKS][SIM868_BUFFER_LENGTH];

uint8_t sim868_status = SIM868_STATUS_OK;
uint8_t sim868_lock = 0;
uint8_t sim868_loopStatus = SIM868_LOOP_INIT;

uint8_t httpStatus = SIM868_HTTP_UNDEFINED;
uint16_t today_filebuffer_index = 0;
char fileReadName[8] = {0};
char fileWriteName[8] = {0};
char fileReadBuffer[255] = {0};
char fileWriteBuffer[255] = {0};
char httpBuffer[255] = {0};
uint8_t hasNewFile = 0;
uint32_t httpTimeout = 0;

uint32_t lastCommandTransmitTimestamp = 0;

char imei[20] = {0};
char cgnurc[115] = {0};
uint32_t cgnurc_timestamp = 0;

void sim868_init(void) {
#   if SIM868_USART == 0
    sim868_usart = &usart0;
#   elif SIM868_USART == 1
    sim868_usart = &usart1;
#   endif
    sim868_usart->rx_vec = sim868_receive;
    usart_init(sim868_usart, 9600);
}

#ifdef SIM868_DEBUG
uint8_t startmsg = 1;
#endif
void sim868_put(char* data) {
#ifdef SIM868_DEBUG
    if (startmsg) {
        usart_print_sync(main_usart, "$ ");
        startmsg = 0;
    }
    usart_print_sync(main_usart, data);
#endif
    usart_print_sync(sim868_usart, data);
}

void sim868_putln(char* data) {
#ifdef SIM868_DEBUG
    if (startmsg) {
        usart_print_sync(main_usart, "$ ");
    }
    usart_println_sync(main_usart, data);
    startmsg = 1;
#endif
    usart_println_sync(sim868_usart, data);
}

void sim868_receive(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
#warning SIM868 USART bridge enabled
    usart_send_sync(main_usart, data);
#else
    uint8_t flag = 0;

    uint8_t *bank = rec_buffer[rec_buffer_bank_write];
    if (rec_buffer_ignore_message) {
        if (data == '\n' || data == '\r') {
            flag = 1;
        }
    } else {
        if (data == '\n' || data == '\r') {
            if (rec_buffer_index > 0) {
                bank[rec_buffer_index] = 0;
                flag = 1;
            }
        } else {
            if (rec_buffer_index < SIM868_BUFFER_LENGTH) {
                bank[rec_buffer_index] = data;
                rec_buffer_index++;
            } else {
                bank[SIM868_BUFFER_LENGTH] = 0;
            }
        }
    }

    if (flag) {
        rec_buffer_ignore_message = 0;
        uint8_t tmp = rec_buffer_bank_write;
        rec_buffer_bank_write++;
        if (rec_buffer_bank_write >= SIM868_BUFFER_BANKS) {
            rec_buffer_bank_write = 0;
        }
        if (rec_buffer_bank_write == rec_buffer_bank_read) {
            rec_buffer_ignore_message = 1;
            rec_buffer_bank_write = tmp;
        }
        rec_buffer_index = 0;
    }
#endif
}

void sim868_handle_buffer(void) {
    if (rec_buffer_bank_read == rec_buffer_bank_write) {
        return;
    }

    cli();
    uint8_t bank[255] = {0};
    strcpy(bank, rec_buffer[rec_buffer_bank_read]);
    rec_buffer_bank_read++;
    if (rec_buffer_bank_read >= SIM868_BUFFER_BANKS) {
        rec_buffer_bank_read = 0;
    }
    sei();

#ifdef SIM868_DEBUG
    usart_print_sync(main_usart, "> ");
    usart_println_sync(main_usart, bank);
#endif

    if (memcmp(bank, "ERROR\0", 6) == 0) {
        sim868_status = SIM868_STATUS_ERROR;
    } else if (memcmp(bank, "OK\0", 3) == 0) {
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "NORMAL POWER DOWN\0", 18) == 0) {
        sim868_loopStatus = SIM868_LOOP_POWER_DOWN;
    } else if (memcmp(bank, "+CPIN: READY", 12) == 0) {
        sim868_loopStatus = SIM868_LOOP_CPIN_READY;
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "+UGNSINF: ", 10) == 0) {
        memset(cgnurc, 0, 115);
        strcpy(cgnurc, bank + 10);
        cgnurc_timestamp = millis();
    } else if (memcmp(bank, "+HTTPACTION: ", 13) == 0) {
        if (memcmp(bank + 15, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else if (memcmp(bank + 15, "60", 2) == 0) {
            httpStatus = SIM868_HTTP_NETWORK_ERROR;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "HTTPACTION: ", 12) == 0) {
        if (memcmp(bank + 14, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else if (memcmp(bank + 14, "60", 2) == 0) {
            httpStatus = SIM868_HTTP_NETWORK_ERROR;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "+SAPBR 1: DEACT", 15) == 0) {
        sim868_status = SIM868_STATUS_OK;
        httpStatus = SIM868_HTTP_UNDEFINED;
    } else if (memcmp(bank, "DOWNLOAD\0", 9) == 0) {
        sim868_status = SIM868_STATUS_OK;
    } else {
        switch (sim868_status) {
            case SIM868_STATUS_WAIT_IMEI:
                memset(imei, 0, 20);
                strcpy(imei, bank);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_FILENAME:
                memset(fileReadName, 0, 8);
                strcpy(fileReadName, bank);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_FILECONTENT:
                memset(fileReadBuffer, 0, 255);
                strcpy(fileReadBuffer, bank);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_BUFFERINDEX:
                sscanf(bank, "%d", &today_filebuffer_index);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_FILEINPUT:
                sim868_status = SIM868_STATUS_OK;
                break;
            default:
                break;
        }
    }
}

void sim868_createFileName(uint16_t fileNameIndex) {
    fileWriteName[3] = 'A' + ((fileNameIndex>>0) & 0x0F);
    fileWriteName[2] = 'A' + ((fileNameIndex>>4) & 0x0F);
    fileWriteName[1] = 'A' + ((fileNameIndex>>8) & 0x0F);
    fileWriteName[0] = 'A' + ((fileNameIndex>>12) & 0x0F);
    fileWriteName[4] = 0;
}

void sim868_http_loop(void) {
#define SIM868_MAIN_LOOP_HTTPINIT   10
#define SIM868_MAIN_LOOP_DELETEFILE 20
#define SIM868_MAIN_LOOP_WAITFILE   30

    static uint8_t ml_status = 0;
    uint16_t len;
    char lens[6] = {0};

    if (sim868_lock & ~SIM868_HTTP_LOCK) {
        return;
    }

    switch (ml_status) {
        case 0:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_FILENAME;
            fileReadName[0] = 0;
            sim868_putln("AT+FSLS=C:\\buffer\\");
            ml_status++;
            break;
        case 1:
            SIM868_async_wait();
            if (sim868_status != SIM868_STATUS_OK) {
                ml_status = 0;
            } else
            if (fileReadName[0] != 0) {
                sim868_status = SIM868_STATUS_WAIT_FILECONTENT;
                sim868_put("AT+FSREAD=C:\\buffer\\");
                sim868_put(fileReadName);
                sim868_putln(",0,255,0");
                ml_status++;
            } else {
                ml_status = SIM868_MAIN_LOOP_WAITFILE;
                hasNewFile = 0;
            }
            break;
        case 2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+SAPBR=1,1");
            ml_status = SIM868_MAIN_LOOP_HTTPINIT;
            break;

        case SIM868_MAIN_LOOP_HTTPINIT:
            SIM868_async_wait();
            len = strlen(fileReadBuffer);
            if (len > 0) {
                SIM868_busy();
                sim868_putln("AT+HTTPINIT");
                ml_status++;
            } else {
                ml_status = SIM868_MAIN_LOOP_DELETEFILE;
            }
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+HTTPPARA=TIMEOUT,60");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+HTTPPARA=URL,");
            sim868_putln(SIM868_TELEMETRY_URL);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+3:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+7; httpStatus = SIM868_HTTP_NETWORK_ERROR; break; }
            sim868_lock |= SIM868_HTTP_LOCK;
            len = strlen(fileReadBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            SIM868_busy();
            sim868_put("AT+HTTPDATA=");
            sim868_put(lens);
            sim868_putln(",2000");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+4:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) {
                ml_status = SIM868_MAIN_LOOP_HTTPINIT+7;
                httpStatus = SIM868_HTTP_NETWORK_ERROR;
                sim868_lock &= ~SIM868_HTTP_LOCK;
                break;
            }
            SIM868_busy();
            sim868_putln(fileReadBuffer);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+5:
            SIM868_async_wait();
            sim868_lock &= ~SIM868_HTTP_LOCK;
            if (sim868_status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+7; httpStatus = SIM868_HTTP_NETWORK_ERROR; break; }
            SIM868_busy();
            httpStatus = SIM868_HTTP_PENDING;
            sim868_putln("AT+HTTPACTION=1");
            httpTimeout = millis();
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+6:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+7; httpStatus = SIM868_HTTP_NETWORK_ERROR; break; }
            if ((millis() - httpTimeout) > 30000) {
                httpStatus = SIM868_HTTP_NETWORK_ERROR;
                usart_println_sync(main_usart, "HTTP TIMEOUT");
            }
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            SIM868_busy();
            sim868_putln("AT+HTTPTERM");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+7:
            SIM868_async_wait();
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            if (httpStatus == SIM868_HTTP_NETWORK_ERROR) {
                ml_status = 2;
            } else {
                ml_status = SIM868_MAIN_LOOP_DELETEFILE;
            }
            break;

        case SIM868_MAIN_LOOP_DELETEFILE:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+FSDEL=C:\\buffer\\");
            sim868_putln(fileReadName);
            ml_status = 0;
            break;

        case SIM868_MAIN_LOOP_WAITFILE:
            if (hasNewFile) {
                ml_status = 0;
            }
            break;
    }
}

void sim868_buffer_loop(void) {
    static uint8_t state = 0;
    uint16_t len;
    char lens[6] = {0};
    char fileindex_str[6] = {0};

    if (sim868_lock & ~SIM868_BUFFER_LOCK) {
        return;
    }

    switch (state) {
        case 0:
            if (httpBuffer[0] != 0) {
                strcpy(fileWriteBuffer, httpBuffer);
                httpBuffer[0] = 0;
                state++;
            }
            break;
        case 1:
            SIM868_async_wait();
            sim868_lock |= SIM868_BUFFER_LOCK;
            SIM868_busy();
            sim868_createFileName(today_filebuffer_index);
            today_filebuffer_index++;
            sim868_put("AT+FSCREATE=C:\\buffer\\");
            sim868_putln(fileWriteName);
            state++;
            break;
        case 2:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) { state = 255; }
            len = strlen(fileWriteBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            sim868_status = SIM868_STATUS_WAIT_FILEINPUT;
            sim868_put("AT+FSWRITE=C:\\buffer\\");
            sim868_put(fileWriteName);
            sim868_put(",0,");
            sim868_put(lens);
            sim868_putln(",60");
            state++;
            break;
        case 3:
            _delay_ms(50);
            SIM868_busy();
            sim868_putln(fileWriteBuffer);
            fileWriteBuffer[0] = 0;
            state++;
            break;
        case 4:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+FSCREATE=C:\\filebuffer_index");
            state++;
            break;
        case 5:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_FILEINPUT;
            sprintf(fileindex_str, "%d", today_filebuffer_index);
            len = strlen(fileindex_str);
            sprintf(lens, "%u", len);
            sim868_put("AT+FSWRITE=C:\\filebuffer_index,0,");
            sim868_put(lens);
            sim868_putln(",60");
            state++;
            break;
        case 6:
            _delay_ms(50);
            SIM868_busy();
            sprintf(fileindex_str, "%d", today_filebuffer_index);
            sim868_putln(fileindex_str);
            state = 255;
            break;

        case 255:
            state = 0;
            sim868_lock &= ~SIM868_BUFFER_LOCK;
            hasNewFile = 1;
            break;
    }
}

void sim868_loop(uint8_t powersave) {
    sim868_handle_buffer();

    if ((millis() - lastCommandTransmitTimestamp) > 10000 &&
        (sim868_status != SIM868_STATUS_OK && sim868_status != SIM868_STATUS_ERROR && sim868_status != SIM868_STATUS_OFF)) {
        usart_println_sync(main_usart, "WATCHDOG");
        sim868_status = SIM868_STATUS_ERROR;
    }

    static uint32_t watchdog = 0;
    static uint8_t prev_status = 0;
    if (prev_status != sim868_loopStatus) {
        watchdog = millis();
        prev_status = sim868_loopStatus;
    }
    if (sim868_loopStatus == SIM868_LOOP_AWAIT && (millis() - watchdog) > 3000) {
        if (powersave) {
            sim868_loopStatus = SIM868_LOOP_POWER_DOWN;
        } else {
            sim868_loopStatus = SIM868_LOOP_INIT;
        }
        watchdog = millis();
    }
    if (!powersave && sim868_loopStatus == SIM868_LOOP_POWER_DOWN && (millis() - watchdog) > 60000) {
        sim868_loopStatus = SIM868_LOOP_INIT;
        watchdog = millis();
    }

    switch (sim868_loopStatus) {
        case SIM868_LOOP_INIT:
            sim868_pwr_on();
            SIM868_busy();
            sim868_putln("AT+CPIN?");
            sim868_loopStatus = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_CPIN_READY:
            imei[0] = 0;
            sim868_loopStatus = SIM868_LOOP_GET_IMEI;
            break;
        case SIM868_LOOP_GET_IMEI:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_IMEI;
            sim868_putln("AT+GSN");
            sim868_loopStatus = SIM868_LOOP_MKBUFFER;
            break;
        case SIM868_LOOP_MKBUFFER:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+FSMKDIR=C:\\buffer\\");
            sim868_loopStatus = SIM868_LOOP_READBUFFERINDEX;
            break;
        case SIM868_LOOP_READBUFFERINDEX:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_BUFFERINDEX;
            sim868_putln("AT+FSREAD=C:\\filebuffer_index,0,6,0");
            sim868_loopStatus = SIM868_LOOP_ENABLE_GNSS_1;
            break;
        case SIM868_LOOP_ENABLE_GNSS_1:
            SIM868_async_wait();

            if (powersave) {
                sim868_loopStatus = SIM868_LOOP_DISABLE_POWER;
                break;
            }

            SIM868_busy();
            sim868_putln("AT+CGNSPWR=1");
            sim868_loopStatus = SIM868_LOOP_ENABLE_GNSS_2;
            break;
        case SIM868_LOOP_ENABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+CGNSURC=");
            sim868_putln(SIM868_CGNURC);
            sim868_loopStatus = SIM868_LOOP_OK;
            break;
        case SIM868_LOOP_POWER_DOWN:
            sim868_status = SIM868_STATUS_OFF;
            sim868_pwr_off();
            if (!powersave) {
                sim868_loopStatus = SIM868_LOOP_INIT;
            }
            break;
        case SIM868_LOOP_POWERSAVE:
            sim868_status = SIM868_STATUS_OFF;
            if (!powersave) {
                sim868_loopStatus = SIM868_LOOP_INIT;
            }
            break;
        case SIM868_LOOP_OK:
            sim868_buffer_loop();
            sim868_http_loop();
            if (powersave) {
                sim868_loopStatus = SIM868_LOOP_DISABLE_GNSS_1;
            }
            break;
        case SIM868_LOOP_DISABLE_GNSS_1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSURC=0");
            sim868_loopStatus = SIM868_LOOP_DISABLE_GNSS_2;
            break;
        case SIM868_LOOP_DISABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSPWR=0");
            sim868_loopStatus = SIM868_LOOP_DISABLE_POWER;
            break;
        case SIM868_LOOP_DISABLE_POWER:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CPOWD=1");
            sim868_loopStatus = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_AWAIT:
            break;
        default:
            sim868_loopStatus = SIM868_LOOP_INIT;
            break;
    }
}

void sim868_post_async(char *data) {
    if (httpBuffer[0] == 0) {
        memset(httpBuffer, 0, 255);
        strcpy(httpBuffer, data);
    } else {
        // Пока что игнорим пакеты, если http буффер занят
    }
}
