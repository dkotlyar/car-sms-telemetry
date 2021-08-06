#include "main.h"
#include "sim868.h"
#include "usart_lib.h"
#include <string.h>
#include <stdio.h>
#include "millis.h"

usart_t *sim868_usart;

#if SIM868_BUFFER_LENGTH <= 0xFF
uint8_t rec_buffer_index = 0;
#else
uint16_t rec_buffer_index = 0;
#endif
uint8_t rec_buffer_bank_write = 0;
uint8_t rec_buffer_bank_read = 0;
uint8_t rec_buffer_ignore_message = 0;
uint8_t rec_buffer[SIM868_BUFFER_BANKS][SIM868_BUFFER_LENGTH];

uint8_t status = SIM868_STATUS_OK;
uint8_t loopStatus = SIM868_LOOP_INIT;

char* pUrl;
uint8_t httpStatus = SIM868_HTTP_UNDEFINED;
char fileName[8] = {0};
char fileBuffer[255] = {0};
char httpBuffer[255] = {0};

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

uint8_t sim868_status(void) {
    return status;
}

void sim868_put(char* data) {
#ifdef SIM868_DEBUG
    usart_print_sync(get_main_usart(), data);
#endif
    usart_print_sync(sim868_usart, data);
}

void sim868_putln(char* data) {
#ifdef SIM868_DEBUG
    usart_println_sync(get_main_usart(), data);
#endif
    usart_println_sync(sim868_usart, data);
}

void sim868_receive(uint8_t data) {
#ifdef SIM868_USART_BRIDGE
#warning SIM868 USART bridge enabled
    usart_send_sync(get_main_usart(), data);
#else
    uint8_t flag = 0;

    uint8_t *bank = rec_buffer[rec_buffer_bank_write];
    if (rec_buffer_ignore_message) {
        if (data == '\n' || data == '\r') {
            flag = 0;
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

usart_t* sim868_get_usart(void) {
    return sim868_usart;
}

void sim868_handle_buffer(void) {
    if (rec_buffer_bank_read == rec_buffer_bank_write) {
        return;
    }
    uint8_t *bank = rec_buffer[rec_buffer_bank_read];
    rec_buffer_bank_read++;
    if (rec_buffer_bank_read >= SIM868_BUFFER_BANKS) {
        rec_buffer_bank_read = 0;
    }

#ifdef SIM868_DEBUG
    usart_println_sync(get_main_usart(), bank);
#endif

    if (memcmp(bank, "ERROR\0", 6) == 0) {
        status = SIM868_STATUS_ERROR;
    } else if (memcmp(bank, "OK\0", 3) == 0) {
        status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "NORMAL POWER DOWN\0", 18) == 0) {
        loopStatus = SIM868_LOOP_POWER_DOWN;
    } else if (memcmp(bank, "+CPIN: READY", 12) == 0) {
        loopStatus = SIM868_LOOP_CPIN_READY;
        status = SIM868_STATUS_OK;
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
        status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "HTTPACTION: ", 12) == 0) {
        if (memcmp(bank + 14, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        status = SIM868_STATUS_OK;
    } else if (memcmp(bank, "+SAPBR 1: DEACT", 15) == 0) {
        status = SIM868_STATUS_OK;
        httpStatus = SIM868_HTTP_UNDEFINED;
    } else if (memcmp(bank, "DOWNLOAD\0", 9) == 0) {
        status = SIM868_STATUS_OK;
    } else {
        switch (status) {
            case SIM868_STATUS_WAIT_IMEI:
                memset(imei, 0, 20);
                strcpy(imei, bank);
                status = SIM868_STATUS_BUSY;
                break;
            case SIM868_STATUS_WAIT_FILENAME:
                memset(fileName, 0, 8);
                strcpy(fileName, bank);
                status = SIM868_STATUS_BUSY;
                break;
            case SIM868_STATUS_WAIT_FILECONTENT:
                memset(fileBuffer, 0, 255);
                strcpy(fileBuffer, bank);
                status = SIM868_STATUS_BUSY;
                break;
            case SIM868_STATUS_WAIT_FILEINPUT:
                status = SIM868_STATUS_OK;
                break;
            default:
                break;
        }
    }
}

void sim868_createFileName(uint16_t fileNameIndex) {
    fileName[3] = 'A' + ((fileNameIndex>>0) & 0x0F);
    fileName[2] = 'A' + ((fileNameIndex>>4) & 0x0F);
    fileName[1] = 'A' + ((fileNameIndex>>8) & 0x0F);
    fileName[0] = 'A' + ((fileNameIndex>>12) & 0x0F);
    fileName[4] = 0;
}

void sim868_http_loop(void) {
#define SIM868_MAIN_LOOP_HTTPINIT   8

    static uint8_t ml_status = 0;
    static uint16_t today_filebuffer_index = 0;
    uint16_t len;
    char lens[6] = {0};

    static uint32_t watchdog = 0;
    static uint32_t httpTimeout = 0;
    static uint8_t prev_ml_status = 0;
    if (prev_ml_status != ml_status) {
        watchdog = millis();
    }
    prev_ml_status = ml_status;
    if ((millis() - watchdog) > 10000 && status == SIM868_STATUS_BUSY ||
            (millis() - watchdog) > 120000) {
        usart_println_sync(get_main_usart(), "WATCHDOG");
        ml_status = 0;
        status = SIM868_STATUS_ERROR;
        watchdog = millis();
        rec_buffer_ignore_message = 0;
        rec_buffer_bank_write = 0;
        rec_buffer_bank_read = 0;
        rec_buffer_index = 0;
    }

    switch (ml_status) {
        case 0:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_putln("AT+FSMKDIR=C:\\buffer\\");
            ml_status++;
            break;
        case 1:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_putln("AT+FSRENAME=C:\\buffer\\today\\,C:\\buffer\\prev\\");
            ml_status++;
            break;
        case 2:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_putln("AT+FSMKDIR=C:\\buffer\\today\\");
            ml_status++;
        case 3:
            SIM868_async_wait();
            status = SIM868_STATUS_WAIT_FILENAME;
            fileName[0] = 0;
            sim868_putln("AT+FSLS=C:\\buffer\\prev\\");
            ml_status++;
            break;
        case 4:
            SIM868_async_wait();
            if (fileName[0] != 0) {
                status = SIM868_STATUS_WAIT_FILECONTENT;
                sim868_put("AT+FSREAD=C:\\buffer\\prev\\");
                sim868_put(fileName);
                sim868_putln(",0,255,0");
                ml_status++;
            } else {
                ml_status = 6;
            }
            break;
        case 5:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_put("AT+FSDEL=C:\\buffer\\prev\\");
            sim868_putln(fileName);
            ml_status = 7;
        case 6:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_putln("AT+FSRMDIR=C:\\buffer\\prev\\");
            ml_status = 20;
            break;

        case 7:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+SAPBR=1,1");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT:
            SIM868_async_wait();
            len = strlen(fileBuffer);
            if (len > 0) {
                status = SIM868_STATUS_BUSY;
                sim868_putln("AT+HTTPINIT");
                ml_status++;
            } else {
                ml_status = 3;
            }
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+1:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_put("AT+HTTPPARA=URL,");
            sim868_putln(pUrl);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+2:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_NETWORK_ERROR; }
            len = strlen(fileBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            status = SIM868_STATUS_BUSY;
            sim868_put("AT+HTTPDATA=");
            sim868_put(lens);
            sim868_putln(",2000");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+3:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_NETWORK_ERROR; }
            status = SIM868_STATUS_BUSY;
            sim868_putln(fileBuffer);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+4:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_NETWORK_ERROR; }
            status = SIM868_STATUS_BUSY;
            httpStatus = SIM868_HTTP_PENDING;
            sim868_putln("AT+HTTPACTION=1");
            httpTimeout = millis();
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+5:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_NETWORK_ERROR; }
            if ((millis() - httpTimeout) > 30000) {
                httpStatus = SIM868_HTTP_NETWORK_ERROR;
                usart_println_sync(get_main_usart(), "Http timeout");
            }
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            status = SIM868_STATUS_BUSY;
            sim868_putln("AT+HTTPTERM");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+6:
            SIM868_async_wait();
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            if (httpStatus == SIM868_HTTP_NETWORK_ERROR) {
                status = SIM868_STATUS_BUSY;
                sim868_createFileName(today_filebuffer_index);
                today_filebuffer_index++;
                sim868_put("AT+FSCREATE=C:\\buffer\\today\\");
                sim868_putln(fileName);
                ml_status++;
            } else {
                fileBuffer[0] = 0;
                ml_status = 3;
            }
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+7:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = 3; }
            len = strlen(fileBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            status = SIM868_STATUS_WAIT_FILEINPUT;
            sim868_put("AT+FSWRITE=C:\\buffer\\today\\");
            sim868_put(fileName);
            sim868_put(",0,");
            sim868_put(lens);
            sim868_putln(",60");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+8:
            _delay_ms(50);
            status = SIM868_STATUS_BUSY;
            sim868_putln(fileBuffer);
            fileBuffer[0] = 0;
            ml_status = 3;
            break;

        case 20:
            SIM868_async_wait();
            if (httpBuffer[0] != 0) {
                strcpy(fileBuffer, httpBuffer);
                httpBuffer[0] = 0;
                ml_status = 7;
            } else {
                status = SIM868_STATUS_WAIT_FILENAME;
                fileName[0] = 0;
                sim868_putln("AT+FSLS=C:\\buffer\\today\\");
                ml_status++;
            }
            break;
        case 21:
            SIM868_async_wait();
            if (fileName[0] != 0) {
                status = SIM868_STATUS_WAIT_FILECONTENT;
                sim868_put("AT+FSREAD=C:\\buffer\\today\\");
                sim868_put(fileName);
                sim868_putln(",0,255,0");
                ml_status++;
            } else {
                ml_status = 23;
            }
            break;
        case 22:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
            sim868_put("AT+FSDEL=C:\\buffer\\today\\");
            sim868_putln(fileName);
            ml_status = 7;
            break;
        case 23:
            if (httpBuffer[0] != 0) {
                ml_status = 20;
            }
            break;
    }
}

void sim868_loop(uint8_t powersave) {
    sim868_handle_buffer();

    static uint32_t watchdog = 0;
    static uint8_t prev_status = 0;
    if (prev_status != loopStatus) {
        watchdog = millis();
        prev_status = loopStatus;
    }
    if (loopStatus == SIM868_LOOP_AWAIT && (millis() - watchdog) > 3000) {
        loopStatus = SIM868_LOOP_INIT;
        watchdog = millis();
        usart_println_sync(get_main_usart(), "sim loop: watchdog");
    }
//    if (!powersave && loopStatus == SIM868_LOOP_POWER_DOWN && (millis() - watchdog) > 60000) {
//        loopStatus = SIM868_LOOP_INIT;
//        watchdog = millis();
//    }

    switch (loopStatus) {
        case SIM868_LOOP_INIT:
            sim868_pwr_on();
            SIM868_busy();
            sim868_putln("AT+CPIN?");
            loopStatus = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_CPIN_READY:
            imei[0] = 0;
            loopStatus = SIM868_LOOP_GET_IMEI;
            break;
        case SIM868_LOOP_GET_IMEI:
            SIM868_async_wait();
            status = SIM868_STATUS_WAIT_IMEI;
            sim868_putln("AT+GSN");
            loopStatus = SIM868_LOOP_ENABLE_GNSS_1;
            break;
        case SIM868_LOOP_ENABLE_GNSS_1:
            SIM868_async_wait();

            if (powersave) {
                loopStatus = SIM868_LOOP_DISABLE_POWER;
                break;
            }

            SIM868_busy();
            sim868_putln("AT+CGNSPWR=1");
            loopStatus = SIM868_LOOP_ENABLE_GNSS_2;
            break;
        case SIM868_LOOP_ENABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_put("AT+CGNSURC=");
            sim868_putln(SIM868_CGNURC);
            loopStatus = SIM868_LOOP_OK;
            break;
        case SIM868_LOOP_POWER_DOWN:
            status = SIM868_STATUS_OFF;
            sim868_pwr_off();
            if (!powersave) {
                loopStatus = SIM868_LOOP_INIT;
            }
            break;
        case SIM868_LOOP_POWERSAVE:
            status = SIM868_STATUS_OFF;
            if (!powersave) {
                loopStatus = SIM868_LOOP_INIT;
            }
            break;
        case SIM868_LOOP_OK:
            sim868_http_loop();
            if (powersave) {
                loopStatus = SIM868_LOOP_DISABLE_GNSS_1;
            }
            break;
        case SIM868_LOOP_DISABLE_GNSS_1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSURC=0");
            loopStatus = SIM868_LOOP_DISABLE_GNSS_2;
            break;
        case SIM868_LOOP_DISABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSPWR=0");
            loopStatus = SIM868_LOOP_DISABLE_POWER;
            break;
        case SIM868_LOOP_DISABLE_POWER:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CPOWD=1");
            loopStatus = SIM868_LOOP_AWAIT;
            break;
        case SIM868_LOOP_AWAIT:
            break;
        default:
            loopStatus = SIM868_LOOP_INIT;
            break;
    }
}

void sim868_enableGnss(void) {
    SIM868_wait();
    sim868_putln("AT+CGNSPWR=1");
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    sim868_putln("AT+CGNSURC=2");
    status = SIM868_STATUS_BUSY;
}

void sim868_httpUrl(char* url) {
    pUrl = url;
}

void sim868_post(char* url, char *data) {
    if (status == SIM868_STATUS_ERROR) {
        status = SIM868_STATUS_OK;
    }

    SIM868_wait();
    if (status == SIM868_STATUS_ERROR) return;
//    usart_println_sync(get_main_usart(), "AT+HTTPINIT");
    sim868_putln("AT+HTTPINIT");
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    if (status == SIM868_STATUS_ERROR) return;
//    usart_print_sync(get_main_usart(), "AT+HTTPPARA=URL,");
    sim868_put("AT+HTTPPARA=URL,");
//    usart_println_sync(get_main_usart(), url);
    sim868_putln(url);
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    if (status == SIM868_STATUS_ERROR) return;
    httpStatus = SIM868_HTTP_READY;

    uint16_t len = strlen(data);
    char lens[6] = {0};
    sprintf(lens, "%d", len);

    SIM868_wait();
    if (status == SIM868_STATUS_ERROR) return;
//    usart_print_sync(get_main_usart(), "AT+HTTPDATA=");
    sim868_put("AT+HTTPDATA=");
//    usart_print_sync(get_main_usart(), lens);
    sim868_put(lens);
//    usart_println_sync(get_main_usart(), ",2000");
    sim868_putln(",2000");
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait DOWNLOAD response
    if (status == SIM868_STATUS_ERROR) return;
    sim868_putln(data);
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait OK response
    if (status == SIM868_STATUS_ERROR) return;
//    usart_println_sync(get_main_usart(), "AT+HTTPACTION=1");
    sim868_putln("AT+HTTPACTION=1");
    status = SIM868_STATUS_BUSY;
    httpStatus = SIM868_HTTP_PENDING;

    while (httpStatus == SIM868_HTTP_PENDING) {_delay_ms(1);}
//    usart_print_sync(get_main_usart(), "HTTP RESULT: ");
//    usart_println_sync(get_main_usart(), httpStatus == SIM868_HTTP_200 ? "200" : "FAILED");
    httpStatus = SIM868_HTTP_READY;

//    usart_println_sync(get_main_usart(), "AT+HTTPTERM");
    sim868_putln("AT+HTTPTERM");
    status = SIM868_STATUS_BUSY;
}

void sim868_post_async(char *data) {
    if (httpBuffer[0] == 0) {
        memset(httpBuffer, 0, 255);
        strcpy(httpBuffer, data);
    } else {
        // Пока что игнорим пакеты, если http буффер занят
    }
}
