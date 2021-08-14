#include "main.h"
#include "sim868.h"
#include "usart_lib.h"
#include <string.h>
#include <stdio.h>
#include "millis.h"

usart_t * sim868_usart;
extern usart_t * main_usart;

#if SIM868_CHARBUFFER_LENGTH <= 0xFF
uint8_t sim868_rec_buffer_index;
#else
uint16_t sim868_rec_buffer_index;
#endif
uint8_t sim868_rec_buffer_bank_write;
uint8_t sim868_rec_buffer_bank_read;
uint8_t sim868_rec_buffer_ignore_message;
char sim868_rec_buffer[SIM868_BUFFER_BANKS][SIM868_CHARBUFFER_LENGTH];

uint8_t sim868_status;
uint8_t sim868_lock;
uint8_t sim868_loop_state;
uint8_t sim868_loop_prevstate;
uint32_t sim868_loopWatchdog;
uint8_t sim868_httploop_state;
uint8_t sim868_bufferloop_state;

uint8_t sim868_httpStatus;
uint16_t sim868_filebuffer_index;
char sim868_fileReadName[SIM868_FILENAME_SIZE] = {0};
char sim868_fileWriteName[SIM868_FILENAME_SIZE] = {0};
char sim868_fileReadBuffer[SIM868_CHARBUFFER_LENGTH] = {0};
char sim868_fileWriteBuffer[SIM868_CHARBUFFER_LENGTH] = {0};
char sim868_httpBuffer[SIM868_CHARBUFFER_LENGTH] = {0};
uint8_t sim868_hasNewFile;
uint32_t sim868_httpActionTime;
uint32_t sim868_httpLoopTime;

uint32_t sim868_lastCommandTransmitTimestamp;

char sim868_imei[SIM868_IMEI_SIZE] = {0};
char sim868_cgnurc[SIM868_CGNURC_SIZE] = {0};
uint32_t sim868_cgnurc_timestamp;

#ifdef SIM868_DEBUG
uint8_t startmsg = 1;
#endif

void sim868_init(void) {
#   if SIM868_USART == 0
    sim868_usart = &usart0;
#   elif SIM868_USART == 1
    sim868_usart = &usart1;
#   endif
    sim868_usart->rx_vec = sim868_receive;
    usart_init(sim868_usart, 57600);
    sim868_reset();
}

void sim868_reset(void) {
    sim868_rec_buffer_index = 0;
    sim868_rec_buffer_bank_write = 0;
    sim868_rec_buffer_bank_read = 0;
    sim868_rec_buffer_ignore_message = 0;
    memset(sim868_rec_buffer, 0, SIM868_BUFFER_BANKS * SIM868_CHARBUFFER_LENGTH);

    sim868_status = SIM868_STATUS_OK;
    sim868_lock = 0;
    sim868_loop_state = SIM868_LOOP_INIT;
    sim868_loop_prevstate = SIM868_LOOP_INIT;
    sim868_httploop_state = 0;
    sim868_bufferloop_state = 0;

    sim868_httpStatus = SIM868_HTTP_UNDEFINED;
    sim868_filebuffer_index = 0;
    memset(sim868_fileReadName, 0, SIM868_FILENAME_SIZE);
    memset(sim868_fileWriteName, 0, SIM868_FILENAME_SIZE);
    memset(sim868_fileReadBuffer, 0, SIM868_CHARBUFFER_LENGTH);
    memset(sim868_fileWriteBuffer, 0, SIM868_CHARBUFFER_LENGTH);
    memset(sim868_httpBuffer, 0, SIM868_CHARBUFFER_LENGTH);
    sim868_hasNewFile = 0;
    sim868_httpActionTime = 0;
    sim868_httpLoopTime = 0;
    sim868_lastCommandTransmitTimestamp = 0;

    memset(sim868_imei, 0, SIM868_IMEI_SIZE);
    memset(sim868_cgnurc, 0, SIM868_CGNURC_SIZE);
    sim868_cgnurc_timestamp = 0;

#ifdef SIM868_DEBUG
    startmsg = 1;
#endif
}

void sim868_put(const char* data) {
#ifdef SIM868_DEBUG
    if (startmsg) {
        usart_print_sync(main_usart, "$ ");
        startmsg = 0;
    }
    usart_print_sync(main_usart, data);
#endif
    usart_print_sync(sim868_usart, data);
}

void sim868_putln(const char* data) {
#ifdef SIM868_DEBUG
    if (startmsg) {
        usart_print_sync(main_usart, "$ ");
        startmsg = 0;
    }
    usart_println_sync(main_usart, data);
    startmsg = 1;
#endif
    usart_println_sync(sim868_usart, data);
    sim868_lastCommandTransmitTimestamp = millis();
}

void sim868_receive(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
#warning SIM868 USART bridge enabled
    usart_send_sync(main_usart, data);
#else
    uint8_t flag = 0;

    if (sim868_rec_buffer_ignore_message) {
        if (data == '\n' || data == '\r') {
            flag = 1;
        }
    } else {
        uint8_t *bank = sim868_rec_buffer[sim868_rec_buffer_bank_write];
        if (data == '\n' || data == '\r') {
            if (sim868_rec_buffer_index > 0) {
                bank[sim868_rec_buffer_index] = 0;
                flag = 1;
            }
        } else {
            if (sim868_rec_buffer_index < SIM868_CHARBUFFER_LENGTH) {
                bank[sim868_rec_buffer_index] = data;
                sim868_rec_buffer_index++;
            } else {
                bank[SIM868_CHARBUFFER_LENGTH] = 0;
            }
        }
    }

    if (flag) {
        sim868_rec_buffer_ignore_message = 0;
        uint8_t tmp = sim868_rec_buffer_bank_write;
        sim868_rec_buffer_bank_write++;
        if (sim868_rec_buffer_bank_write >= SIM868_BUFFER_BANKS) {
            sim868_rec_buffer_bank_write = 0;
        }
        if (sim868_rec_buffer_bank_write == sim868_rec_buffer_bank_read) {
            sim868_rec_buffer_ignore_message = 1;
            sim868_rec_buffer_bank_write = tmp;
        }
        sim868_rec_buffer_index = 0;
    }
#endif
}

void sim868_handle_buffer(void) {
    if (sim868_rec_buffer_bank_read == sim868_rec_buffer_bank_write) {
        return;
    }

    cli();
    char bank[SIM868_CHARBUFFER_LENGTH] = {0};
    strcpy(bank, sim868_rec_buffer[sim868_rec_buffer_bank_read]);
    sim868_rec_buffer_bank_read++;
    if (sim868_rec_buffer_bank_read >= SIM868_BUFFER_BANKS) {
        sim868_rec_buffer_bank_read = 0;
    }
    sei();

    sim868_debug("> ");
    sim868_debugln(bank);

    if (memcmp(bank, "ERROR\0", 6) == 0) {
        sim868_status = SIM868_STATUS_ERROR;
    } else if (memcmp(bank, "OK\0", 3) == 0) {
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "NORMAL POWER DOWN\0", 18) == 0) {
        sim868_loop_state = SIM868_LOOP_POWER_DOWN;
    } else if (memcmp(bank, "+CPIN: READY", 12) == 0) {
        sim868_loop_state = SIM868_LOOP_CPIN_READY;
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "+UGNSINF: ", 10) == 0) {
        memset(sim868_cgnurc, 0, 115);
        strcpy(sim868_cgnurc, bank + 10);
        sim868_cgnurc_timestamp = millis();
    } else if (memcmp(bank, "+HTTPACTION: ", 13) == 0) {
        if (memcmp(bank + 15, "200", 3) == 0) {
            sim868_httpStatus = SIM868_HTTP_200;
        } else if (memcmp(bank + 15, "60", 2) == 0) {
            sim868_httpStatus = SIM868_HTTP_NETWORK_ERROR;
        } else {
            sim868_httpStatus = SIM868_HTTP_FAILED;
        }
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "HTTPACTION: ", 12) == 0) {
        if (memcmp(bank + 14, "200", 3) == 0) {
            sim868_httpStatus = SIM868_HTTP_200;
        } else if (memcmp(bank + 14, "60", 2) == 0) {
            sim868_httpStatus = SIM868_HTTP_NETWORK_ERROR;
        } else {
            sim868_httpStatus = SIM868_HTTP_FAILED;
        }
        sim868_status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "+SAPBR 1: DEACT", 15) == 0) {
        sim868_status = SIM868_STATUS_OK;
        sim868_httpStatus = SIM868_HTTP_UNDEFINED;
    } else if (memcmp(bank, "DOWNLOAD\0", 9) == 0) {
        sim868_status = SIM868_STATUS_OK;
    } else {
        switch (sim868_status) {
            case SIM868_STATUS_WAIT_IMEI:
                memset(sim868_imei, 0, 20);
                strcpy(sim868_imei, bank);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_FILENAME:
                memset(sim868_fileReadName, 0, 8);
                strcpy(sim868_fileReadName, bank);
                sim868_status = SIM868_STATUS_IGNORE;
                break;
            case SIM868_STATUS_WAIT_FILECONTENT:
                memset(sim868_fileReadBuffer, 0, SIM868_CHARBUFFER_LENGTH);
                strcpy(sim868_fileReadBuffer, bank);
                SIM868_busy();
                break;
            case SIM868_STATUS_WAIT_BUFFERINDEX:
                sscanf(bank, "%d", &sim868_filebuffer_index);
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
    sim868_fileWriteName[3] = 'A' + ((fileNameIndex >> 0) & 0x0F);
    sim868_fileWriteName[2] = 'A' + ((fileNameIndex >> 4) & 0x0F);
    sim868_fileWriteName[1] = 'A' + ((fileNameIndex >> 8) & 0x0F);
    sim868_fileWriteName[0] = 'A' + ((fileNameIndex >> 12) & 0x0F);
    sim868_fileWriteName[4] = 0;
}

void sim868_http_loop(void) {
#define SIM868_MAIN_LOOP_HTTPINIT   10
#define SIM868_MAIN_LOOP_DELETEFILE 20
#define SIM868_MAIN_LOOP_WAITFILE   30
#define SIM868_checkHttpError() { \
    if (sim868_status == SIM868_STATUS_ERROR) { \
        sim868_httploop_state = SIM868_MAIN_LOOP_HTTPINIT+7; \
        sim868_httpStatus = SIM868_HTTP_NETWORK_ERROR;        \
        break; \
    } \
}

    uint16_t len;
    char lens[6] = {0};
    char tmp[20] = {0};

    if (sim868_lock & ~SIM868_HTTP_LOCK) {
        return;
    }

    switch (sim868_httploop_state) {
        case 0:
            SIM868_async_wait();
            sim868_httpLoopTime = millis();
            sim868_status = SIM868_STATUS_WAIT_FILENAME;
            sim868_fileReadName[0] = 0;
            sim868_putln("AT+FSLS=C:\\buffer\\");
            sim868_httploop_state++;
            break;
        case 1:
            SIM868_async_wait();
            if (sim868_status != SIM868_STATUS_OK) {
                sim868_httploop_state = 0;
            } else
            if (sim868_fileReadName[0] != 0) {
                sim868_status = SIM868_STATUS_WAIT_FILECONTENT;
                sim868_put("AT+FSREAD=C:\\buffer\\");
                sim868_put(sim868_fileReadName);
                sim868_put(",0,");
                sprintf(lens, "%u", SIM868_CHARBUFFER_LENGTH);
                sim868_put(lens);
                sim868_putln(",0");
                sim868_httploop_state++;
            } else {
                sim868_httploop_state = SIM868_MAIN_LOOP_WAITFILE;
                sim868_hasNewFile = 0;
            }
            break;
        case 2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+SAPBR=1,1");
            sim868_httploop_state = SIM868_MAIN_LOOP_HTTPINIT;
            break;

        case SIM868_MAIN_LOOP_HTTPINIT:
            SIM868_async_wait();
            len = strlen(sim868_fileReadBuffer);
            if (len > 0) {
                SIM868_busy();
                sim868_putln("AT+HTTPINIT");
                sim868_httploop_state++;
            } else {
                sim868_httploop_state = SIM868_MAIN_LOOP_DELETEFILE;
            }
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+HTTPPARA=TIMEOUT,60");
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+HTTPPARA=URL,");
            sim868_putln(SIM868_TELEMETRY_URL);
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+3:
            SIM868_async_wait();
            SIM868_checkHttpError();
            sim868_lock |= SIM868_HTTP_LOCK;
            len = strlen(sim868_fileReadBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            SIM868_busy();
            sim868_put("AT+HTTPDATA=");
            sim868_put(lens);
            sim868_putln(",2000");
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+4:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) {
                sim868_httploop_state = SIM868_MAIN_LOOP_HTTPINIT + 7;
                sim868_httpStatus = SIM868_HTTP_NETWORK_ERROR;
                sim868_lock &= ~SIM868_HTTP_LOCK;
                break;
            }
            SIM868_busy();
            sim868_putln(sim868_fileReadBuffer);
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+5:
            SIM868_async_wait();
            sim868_lock &= ~SIM868_HTTP_LOCK;
            SIM868_checkHttpError();
            SIM868_busy();
            sim868_httpStatus = SIM868_HTTP_PENDING;
            sim868_putln("AT+HTTPACTION=1");
            sim868_httpActionTime = millis();
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+6:
            SIM868_async_wait();
            SIM868_checkHttpError();
            if ((millis() - sim868_httpActionTime) > 30000) {
                sim868_httpStatus = SIM868_HTTP_NETWORK_ERROR;
                sim868_debugln("HTTP TIMEOUT");
            }
            if (sim868_httpStatus == SIM868_HTTP_PENDING) {break;}

            sprintf(tmp, "%lu", (millis() - sim868_httpActionTime));
            sim868_debug("! HTTP action time: ");
            sim868_debugln(tmp);

            SIM868_busy();
            sim868_putln("AT+HTTPTERM");
            sim868_httploop_state++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+7:
            SIM868_async_wait();
            if (sim868_httpStatus == SIM868_HTTP_PENDING) {break;}
            if (sim868_httpStatus == SIM868_HTTP_NETWORK_ERROR) {
                sim868_httploop_state = 2;
            } else {
                sim868_httploop_state = SIM868_MAIN_LOOP_DELETEFILE;
            }
            break;

        case SIM868_MAIN_LOOP_DELETEFILE:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+FSDEL=C:\\buffer\\");
            sim868_putln(sim868_fileReadName);

            sprintf(tmp, "%lu", (millis() - sim868_httpLoopTime));
            sim868_debug("! HTTP Loop time: ");
            sim868_debugln(tmp);
            sim868_httploop_state = 0;
            break;

        case SIM868_MAIN_LOOP_WAITFILE:
            if (sim868_hasNewFile) {
                sim868_httploop_state = 0;
            }
            break;
    }
}

void sim868_buffer_loop(void) {
    uint16_t len;
    char lens[6] = {0};
    char fileindex_str[6] = {0};

    if (sim868_lock & ~SIM868_BUFFER_LOCK) {
        return;
    }

    switch (sim868_bufferloop_state) {
        case 0:
            if (sim868_httpBuffer[0] != 0) {
                strcpy(sim868_fileWriteBuffer, sim868_httpBuffer);
                sim868_httpBuffer[0] = 0;
                sim868_bufferloop_state++;
            }
            break;
        case 1:
            SIM868_async_wait();
            sim868_lock |= SIM868_BUFFER_LOCK;
            SIM868_busy();
            sim868_createFileName(sim868_filebuffer_index);
            sim868_filebuffer_index++;
            sim868_put("AT+FSCREATE=C:\\buffer\\");
            sim868_putln(sim868_fileWriteName);
            sim868_bufferloop_state++;
            break;
        case 2:
            SIM868_async_wait();
            if (sim868_status == SIM868_STATUS_ERROR) { sim868_bufferloop_state = 255; }
            len = strlen(sim868_fileWriteBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            sim868_status = SIM868_STATUS_WAIT_FILEINPUT;
            sim868_put("AT+FSWRITE=C:\\buffer\\");
            sim868_put(sim868_fileWriteName);
            sim868_put(",0,");
            sim868_put(lens);
            sim868_putln(",60");
            sim868_bufferloop_state++;
            break;
        case 3:
            _delay_ms(50);
            SIM868_busy();
            sim868_putln(sim868_fileWriteBuffer);
            sim868_fileWriteBuffer[0] = 0;
            sim868_bufferloop_state++;
            break;
        case 4:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+FSCREATE=C:\\filebuffer_index");
            sim868_bufferloop_state++;
            break;
        case 5:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_FILEINPUT;
            sprintf(fileindex_str, "%d", sim868_filebuffer_index);
            len = strlen(fileindex_str);
            sprintf(lens, "%u", len);
            sim868_put("AT+FSWRITE=C:\\filebuffer_index,0,");
            sim868_put(lens);
            sim868_putln(",60");
            sim868_bufferloop_state++;
            break;
        case 6:
            _delay_ms(50);
            SIM868_busy();
            sprintf(fileindex_str, "%d", sim868_filebuffer_index);
            sim868_putln(fileindex_str);
            sim868_bufferloop_state = 255;
            break;

        case 255:
            sim868_bufferloop_state = 0;
            sim868_lock &= ~SIM868_BUFFER_LOCK;
            sim868_hasNewFile = 1;
            break;
    }
}

uint8_t sim868_loop(uint8_t powersave) {
    uint8_t retval = 0;
    sim868_handle_buffer();

    if ((millis() - sim868_lastCommandTransmitTimestamp) > 1000 && sim868_status == SIM868_STATUS_IGNORE) {
        sim868_debugln("WATCHDOG");
        sim868_status = SIM868_STATUS_OK;
    }
    if ((millis() - sim868_lastCommandTransmitTimestamp) > 5000 &&
        (sim868_status != SIM868_STATUS_OK && sim868_status != SIM868_STATUS_ERROR && sim868_status != SIM868_STATUS_OFF)) {
        sim868_debugln("WATCHDOG");
        if (sim868_status == SIM868_STATUS_IGNORE) {
            sim868_status = SIM868_STATUS_OK;
        } else {
            sim868_status = SIM868_STATUS_ERROR;
        }
    }

    if (sim868_loop_prevstate != sim868_loop_state) {
        sim868_loopWatchdog = millis();
        sim868_loop_prevstate = sim868_loop_state;
    }
    if (sim868_loop_state == SIM868_LOOP_AWAIT && (millis() - sim868_loopWatchdog) > 3000) {
        if (powersave) {
            sim868_loop_state = SIM868_LOOP_POWER_DOWN;
        } else {
            sim868_loop_state = SIM868_LOOP_INIT;
        }
        sim868_loopWatchdog = millis();
    }
    if (!powersave && sim868_loop_state == SIM868_LOOP_POWER_DOWN && (millis() - sim868_loopWatchdog) > 60000) {
        sim868_loop_state = SIM868_LOOP_INIT;
        sim868_loopWatchdog = millis();
    }

    switch (sim868_loop_state) {
        case SIM868_LOOP_INIT:
            if (powersave) {
                sim868_loop_state = SIM868_LOOP_POWER_DOWN;
                break;
            }
            sim868_pwr_on();
            SIM868_busy();
            sim868_putln("AT+CPIN?");
            sim868_loop_state = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_CPIN_READY:
            sim868_imei[0] = 0;
            sim868_loop_state = SIM868_LOOP_GET_IMEI;
            break;
        case SIM868_LOOP_GET_IMEI:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_IMEI;
            sim868_putln("AT+GSN");
            sim868_loop_state = SIM868_LOOP_MKBUFFER;
            break;
        case SIM868_LOOP_MKBUFFER:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+FSMKDIR=C:\\buffer\\");
            sim868_loop_state = SIM868_LOOP_READBUFFERINDEX;
            break;
        case SIM868_LOOP_READBUFFERINDEX:
            SIM868_async_wait();
            sim868_status = SIM868_STATUS_WAIT_BUFFERINDEX;
            sim868_putln("AT+FSREAD=C:\\filebuffer_index,0,6,0");
            sim868_loop_state = SIM868_LOOP_ENABLE_GNSS_1;
            break;
        case SIM868_LOOP_ENABLE_GNSS_1:
            SIM868_async_wait();

            if (powersave) {
                sim868_loop_state = SIM868_LOOP_DISABLE_POWER;
                break;
            }

            SIM868_busy();
            sim868_putln("AT+CGNSPWR=1");
            sim868_loop_state = SIM868_LOOP_ENABLE_GNSS_2;
            break;
        case SIM868_LOOP_ENABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+CGNSURC=");
            sim868_putln(SIM868_CGNURC);
            sim868_loop_state = SIM868_LOOP_OK;
            break;
        case SIM868_LOOP_POWER_DOWN:
            sim868_status = SIM868_STATUS_OFF;
            sim868_pwr_off();
            if (!powersave) {
                sim868_loop_state = SIM868_LOOP_INIT;
                break;
            }
            retval = 1;
            break;
        case SIM868_LOOP_OK:
            sim868_buffer_loop();
            sim868_http_loop();
            if (powersave) {
                sim868_loop_state = SIM868_LOOP_DISABLE_GNSS_1;
            }
            break;
        case SIM868_LOOP_DISABLE_GNSS_1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSURC=0");
            sim868_loop_state = SIM868_LOOP_DISABLE_GNSS_2;
            break;
        case SIM868_LOOP_DISABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSPWR=0");
            sim868_loop_state = SIM868_LOOP_DISABLE_POWER;
            break;
        case SIM868_LOOP_DISABLE_POWER:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CPOWD=1");
            sim868_loop_state = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_AWAIT:
            break;
        default:
            sim868_loop_state = SIM868_LOOP_INIT;
            break;
    }

    return retval;
}

void sim868_post_async(const char *data) {
    if (sim868_httpBuffer[0] == 0) {
        memset(sim868_httpBuffer, 0, SIM868_CHARBUFFER_LENGTH);
        strcpy(sim868_httpBuffer, data);
    } else {
        // Пока что игнорим пакеты, если http буффер занят
    }
}
