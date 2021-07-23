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
uint8_t rec_buffer[SIM868_BUFFER_LENGTH];

uint8_t status = SIM868_STATUS_OK;
uint8_t loopStatus = SIM868_LOOP_INIT;

char* pUrl;
uint8_t httpStatus = SIM868_HTTP_UNDEFINED;
char fileName[8] = {0};
char fileBuffer[255] = {0};
char httpBuffer[255] = {0};

char imei[20] = {0};
char cgnurc[115] = {0};

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
    if (data == '\n' || data == '\r') {
        rec_buffer[rec_buffer_index] = 0;
        sim868_handle_buffer();
    } else {
        rec_buffer[rec_buffer_index] = data;
        rec_buffer_index++;
        if (rec_buffer_index == SIM868_BUFFER_LENGTH) {
            rec_buffer[SIM868_BUFFER_LENGTH] = 0;
            sim868_handle_buffer();
        }
    }
#endif
}

usart_t* sim868_get_usart(void) {
    return sim868_usart;
}

void sim868_handle_buffer(void) {
    if (rec_buffer_index == 0) {
        return;
    }

#ifdef SIM868_DEBUG
    usart_println_sync(get_main_usart(), rec_buffer);
#endif

    if (memcmp(rec_buffer, "ERROR\0", 6) == 0) {
        status = SIM868_STATUS_ERROR;
//        usart_print_sync(get_main_usart(), "AT:");
//        usart_println_sync(get_main_usart(), rec_buffer);
    } else if (memcmp(rec_buffer, "OK\0", 3) == 0) {
        status = SIM868_STATUS_OK;
//        usart_print_sync(get_main_usart(), "AT:");
//        usart_println_sync(get_main_usart(), rec_buffer);
    } else if (memcmp(rec_buffer, "NORMAL POWER DOWN\0", 18) == 0) {
        loopStatus = SIM868_LOOP_POWER_DOWN;
//        usart_println_sync(get_main_usart(), rec_buffer);
    } else if (memcmp(rec_buffer, "+CPIN: READY", 12) == 0) {
        loopStatus = SIM868_LOOP_CPIN_READY;
        status = SIM868_STATUS_OK;
//        usart_println_sync(get_main_usart(), rec_buffer);
    } else if (memcmp(rec_buffer, "+UGNSINF: ", 10) == 0) {
        memcpy(cgnurc, rec_buffer + 10, rec_buffer_index - 10);
    } else if (memcmp(rec_buffer, "+HTTPACTION: ", 13) == 0) {
        if (memcmp(rec_buffer + 15, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        status = SIM868_STATUS_OK;
    } else if (memcmp(rec_buffer, "HTTPACTION: ", 12) == 0) {
        if (memcmp(rec_buffer + 14, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        status = SIM868_STATUS_OK;
    } else if (memcmp(rec_buffer, "+SAPBR 1: DEACT", 15) == 0) {
//        usart_println_sync(get_main_usart(), "HTTP FAILED");
        status = SIM868_STATUS_OK;
        httpStatus = SIM868_HTTP_UNDEFINED;
    } else if (memcmp(rec_buffer, "DOWNLOAD\0", 9) == 0) {
        status = SIM868_STATUS_OK;
    } else {
        switch (status) {
            case SIM868_STATUS_WAIT_IMEI:
                memcpy(imei, rec_buffer, rec_buffer_index);
                imei[rec_buffer_index] = 0;
                status = SIM868_STATUS_OK;
                break;
            case SIM868_STATUS_WAIT_FILENAME:
                memcpy(fileName, rec_buffer, rec_buffer_index);
                fileName[rec_buffer_index] = 0;
                status = SIM868_STATUS_BUSY;
                break;
            case SIM868_STATUS_WAIT_FILECONTENT:
                memcpy(fileBuffer, rec_buffer, rec_buffer_index);
                fileBuffer[rec_buffer_index] = 0;
                status = SIM868_STATUS_BUSY;
                break;
            case SIM868_STATUS_WAIT_FILEINPUT:
//                usart_println_sync(get_main_usart(), rec_buffer);
                status = SIM868_STATUS_OK;
                break;
            default:
//                usart_println_sync(get_main_usart(), rec_buffer);
//                status = SIM868_STATUS_BUSY;
                break;
        }
    }

    rec_buffer_index = 0;
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
    }

    switch (ml_status) {
        case 0:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Create buffer");
            sim868_putln("AT+FSMKDIR=C:\\buffer\\");
            ml_status++;
            break;
        case 1:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Move today to prev");
            sim868_putln("AT+FSRENAME=C:\\buffer\\today\\,C:\\buffer\\prev\\");
            ml_status++;
            break;
        case 2:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Create today dir");
            sim868_putln("AT+FSMKDIR=C:\\buffer\\today\\");
            ml_status++;
        case 3:
            SIM868_async_wait();
            status = SIM868_STATUS_WAIT_FILENAME;
            fileName[0] = 0;
//            usart_println_sync(get_main_usart(), "Get prev files");
            sim868_putln("AT+FSLS=C:\\buffer\\prev\\");
            ml_status++;
            break;
        case 4:
            SIM868_async_wait();
            if (fileName[0] != 0) {
                status = SIM868_STATUS_WAIT_FILECONTENT;
//                usart_print_sync(get_main_usart(), "Get file content: ");
//                usart_println_sync(get_main_usart(), fileName);
                sim868_put("AT+FSREAD=C:\\buffer\\prev\\");
                sim868_put(fileName);
                sim868_putln(",0,255,0");
                ml_status++;
            } else {
//                usart_println_sync(get_main_usart(), "No files, goto 15");
                ml_status = 6;
            }
            break;
        case 5:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Del file");
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
//                usart_println_sync(get_main_usart(), "Http init");
                sim868_putln("AT+HTTPINIT");
                ml_status++;
            } else {
//                usart_println_sync(get_main_usart(), "File empty");
                ml_status = 3;
            }
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+1:
            SIM868_async_wait();
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Http url");
            sim868_put("AT+HTTPPARA=URL,");
            sim868_putln(pUrl);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+2:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_FAILED; }
            len = strlen(fileBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Http data");
            sim868_put("AT+HTTPDATA=");
            sim868_put(lens);
            sim868_putln(",2000");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+3:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_FAILED; }
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Http post: ");
//            usart_println_sync(get_main_usart(), fileBuffer);
            sim868_putln(fileBuffer);
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+4:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_FAILED; }
            status = SIM868_STATUS_BUSY;
            httpStatus = SIM868_HTTP_PENDING;
//            usart_println_sync(get_main_usart(), "Http action");
            sim868_putln("AT+HTTPACTION=1");
            ml_status++;
//            ml_status = SIM868_MAIN_LOOP_HTTPINIT+6;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+5:
            SIM868_async_wait();
            if (status == SIM868_STATUS_ERROR) { ml_status = SIM868_MAIN_LOOP_HTTPINIT+6; httpStatus = SIM868_HTTP_FAILED; }
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Http term");
            sim868_putln("AT+HTTPTERM");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+6:
            SIM868_async_wait();
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            if (httpStatus != SIM868_HTTP_200) {
                status = SIM868_STATUS_BUSY;
                sim868_createFileName(today_filebuffer_index);
                today_filebuffer_index++;
//                usart_println_sync(get_main_usart(), "Http fail, create file");
                sim868_put("AT+FSCREATE=C:\\buffer\\today\\");
                sim868_putln(fileName);
                ml_status++;
            } else {
                fileBuffer[0] = 0;
//                usart_println_sync(get_main_usart(), "Http 200, goto 3");
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
//            usart_print_sync(get_main_usart(), "Write file, bytes: ");
//            usart_println_sync(get_main_usart(), lens);
            sim868_put("AT+FSWRITE=C:\\buffer\\today\\");
            sim868_put(fileName);
            sim868_put(",0,");
            sim868_put(lens);
            sim868_putln(",60");
            ml_status++;
            break;
        case SIM868_MAIN_LOOP_HTTPINIT+8:
            _delay_ms(50);
//            SIM868_async_wait();
//            if (status == SIM868_STATUS_ERROR) { ml_status = 3; }
            status = SIM868_STATUS_BUSY;
//            usart_println_sync(get_main_usart(), "Transmit file data");
            sim868_putln(fileBuffer);
            fileBuffer[0] = 0;
            ml_status = 3;
            break;

        case 20:
            SIM868_async_wait();
            if (httpBuffer[0] != 0) {
//                usart_println_sync(get_main_usart(), "Send new http post");
                strcpy(fileBuffer, httpBuffer);
                httpBuffer[0] = 0;
                ml_status = 7;
            } else {
//                usart_println_sync(get_main_usart(), "Check failed today");
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
//                usart_println_sync(get_main_usart(), "Get today file content");
                sim868_put("AT+FSREAD=C:\\buffer\\today\\");
                sim868_put(fileName);
                sim868_putln(",0,255,0");
                ml_status++;
            } else {
//                usart_println_sync(get_main_usart(), "No today files, goto 20");
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

void sim868_loop(void) {
    static uint32_t watchdog = 0;
    static uint8_t prev_status = 0;
    if (prev_status != loopStatus) {
        watchdog = millis();
        prev_status = loopStatus;
    }
    if (loopStatus == SIM868_LOOP_AWAIT && (millis() - watchdog) > 3000) {
        loopStatus = SIM868_LOOP_INIT;
        watchdog = millis();
    }
    if (loopStatus == SIM868_LOOP_POWER_DOWN && (millis() - watchdog) > 60000) {
        loopStatus = SIM868_LOOP_INIT;
        watchdog = millis();
    }

    switch (loopStatus) {
        case SIM868_LOOP_INIT:
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
//            usart_println_sync(get_main_usart(), "AT+GSN");
            status = SIM868_STATUS_WAIT_IMEI;
            sim868_putln("AT+GSN");
//            sim868_enableGnss();
            loopStatus = SIM868_LOOP_ENABLE_GNSS_1;
            break;
        case SIM868_LOOP_ENABLE_GNSS_1:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSPWR=1");
            loopStatus = SIM868_LOOP_ENABLE_GNSS_2;
            break;
        case SIM868_LOOP_ENABLE_GNSS_2:
            SIM868_async_wait();
            SIM868_busy();
            sim868_putln("AT+CGNSURC=5");
            loopStatus = SIM868_LOOP_OK;
            break;
        case SIM868_LOOP_POWER_DOWN:
            status = SIM868_STATUS_OFF;
            break;
        case SIM868_LOOP_OK:
            sim868_http_loop();
            break;
        case SIM868_LOOP_AWAIT:
            break;
        default:
            loopStatus = SIM868_LOOP_INIT;
            break;
    }
}

void sim868_enableGnss(void) {
//    usart_println_sync(get_main_usart(), "enable gnss wait");
    SIM868_wait();
//    usart_println_sync(get_main_usart(), "AT+CGNSPWR=1");
    sim868_putln("AT+CGNSPWR=1");
    status = SIM868_STATUS_BUSY;

//    usart_println_sync(get_main_usart(), "enable cgnsurc wait");
    SIM868_wait();
//    usart_println_sync(get_main_usart(), "AT+CGNSURC=2");
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
        strcpy(httpBuffer, data);
    } else {
        // Пока что игнорим пакеты, если http буффер занят
    }
}
