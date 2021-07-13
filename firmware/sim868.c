#include "main.h"
#include "sim868.h"
#include "usart_lib.h"
#include <string.h>
#include <stdio.h>

usart_t *sim868_usart;

#if SIM868_BUFFER_LENGTH <= 0xFF
uint8_t rec_buffer_index = 0;
#else
uint16_t rec_buffer_index = 0;
#endif
uint8_t rec_buffer[SIM868_BUFFER_LENGTH];

uint8_t status = SIM868_STATUS_FREE;
uint8_t loopStatus = SIM868_LOOP_INIT;

char* pUrl;
uint8_t httpStatus = SIM868_HTTP_UNDEFINED;
char fileName[8] = {0};
char fileBuffer[255] = {0};
char* httpBuffer = 0;

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

    if (memcmp(rec_buffer, "ERROR", 5) == 0 || memcmp(rec_buffer, "OK", 2) == 0) {
        status = SIM868_STATUS_FREE;
        usart_print_sync(get_main_usart(), "AT:");
        usart_println_sync(get_main_usart(), rec_buffer);
    } else if (memcmp(rec_buffer, "NORMAL POWER DOWN\0", 18) == 0) {
        loopStatus = SIM868_LOOP_POWER_DOWN;
        usart_println_sync(get_main_usart(), "Power down");
    } else if (memcmp(rec_buffer, "+CPIN: READY", 12) == 0) {
        loopStatus = SIM868_LOOP_CPIN_READY;
    } else if (memcmp(rec_buffer, "+UGNSINF: ", 10) == 0) {
        memcpy(cgnurc, rec_buffer + 10, rec_buffer_index - 10);
    } else if (memcmp(rec_buffer, "+HTTPACTION: ", 13) == 0) {
        if (memcmp(rec_buffer + 15, "200", 3) == 0) {
            httpStatus = SIM868_HTTP_200;
        } else {
            httpStatus = SIM868_HTTP_FAILED;
        }
        status = SIM868_STATUS_FREE;
    } else if (memcmp(rec_buffer, "+SAPBR 1: DEACT", 15) == 0) {
        usart_println_sync(get_main_usart(), "HTTP FAILED");
        status = SIM868_STATUS_FREE;
        httpStatus = SIM868_HTTP_UNDEFINED;
    } else {
        switch (status) {
            case SIM868_STATUS_WAITIMEI:
                memcpy(imei, rec_buffer, rec_buffer_index);
                status = SIM868_STATUS_FREE;
                break;
            case SIM868_STATUS_WAIT_FILENAME:
                memcpy(fileName, rec_buffer, rec_buffer_index);
                break;
            case SIM868_STATUS_WAIT_FILECONTENT:
                memcpy(fileBuffer, rec_buffer, rec_buffer_index);
                status = SIM868_STATUS_FREE;
                break;
            case SIM868_STATUS_WAIT_FILEINPUT:
                status = SIM868_STATUS_FREE;
                break;
            default:
                usart_println_sync(get_main_usart(), rec_buffer);
                status = SIM868_STATUS_FREE;
                break;
        }
    }

    rec_buffer_index = 0;
}

void sim868_createFileName(uint16_t fileNameIndex) {
    fileName[0] = 'A' + ((fileNameIndex>>0) & 0x0F);
    fileName[1] = 'A' + ((fileNameIndex>>4) & 0x0F);
    fileName[2] = 'A' + ((fileNameIndex>>8) & 0x0F);
    fileName[3] = 'A' + ((fileNameIndex>>12) & 0x0F);
    fileName[4] = 0;
}

void sim868_main_loop(void) {
    static uint8_t ml_status = 0;
    static uint16_t today_filebuffer_index = 0;
    uint16_t len;
    char lens[6] = {0};

    switch (ml_status) {
        case 0:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Create buffer");
            usart_println_sync(sim868_usart, "AT+FSMKDIR=C:\\buffer\\");
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 1:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Move today to prev");
            usart_println_sync(sim868_usart, "AT+FSRENAME=C:\\buffer\\today\\,C:\\buffer\\prev\\");
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 2:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Create today dir");
            usart_println_sync(sim868_usart, "AT+FSMKDIR=C:\\buffer\\today\\");
            status = SIM868_STATUS_BUSY;
            ml_status++;
        case 3:
            SIM868_async_wait();
            fileName[0] = 0;
            usart_println_sync(get_main_usart(), "Get prev files");
            usart_println_sync(sim868_usart, "AT+FSLS=C:\\buffer\\prev\\");
            status = SIM868_STATUS_WAIT_FILENAME;
            ml_status++;
            break;
        case 4:
            SIM868_async_wait();
            if (fileName[0] != 0) {
                usart_print_sync(get_main_usart(), "Get file content: ");
                usart_println_sync(get_main_usart(), fileName);
                usart_print_sync(sim868_usart, "AT+FSREAD=C:\\buffer\\prev\\");
                usart_print_sync(sim868_usart, fileName);
                usart_println_sync(sim868_usart, ",0,255,0");
                status = SIM868_STATUS_WAIT_FILECONTENT;
                ml_status++;
            } else {
                usart_println_sync(get_main_usart(), "No files, goto 15");
                ml_status = 15;
            }
            break;
        case 5:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Del file");
            usart_print_sync(sim868_usart, "AT+FSDEL=C:\\buffer\\prev\\");
            usart_println_sync(sim868_usart, fileName);
            status = SIM868_STATUS_BUSY;
            ml_status++;
        case 6:
            SIM868_async_wait();
            len = strlen(fileBuffer);
            if (len > 0) {
                usart_println_sync(get_main_usart(), "Http init");
                usart_println_sync(sim868_usart, "AT+HTTPINIT");
                status = SIM868_STATUS_BUSY;
                ml_status++;
            } else {
                usart_println_sync(get_main_usart(), "File empty");
                ml_status = 3;
            }
            break;
        case 7:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Http url");
            usart_print_sync(sim868_usart, "AT+HTTPPARA=URL,");
            usart_println_sync(sim868_usart, pUrl);
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 8:
            SIM868_async_wait();
            len = strlen(fileBuffer);
            lens[0] = 0;
            sprintf(lens, "%u", len);
            usart_println_sync(get_main_usart(), "Http data");
            usart_print_sync(sim868_usart, "AT+HTTPDATA=");
            usart_print_sync(sim868_usart, lens);
            usart_println_sync(sim868_usart, ",2000");
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 9:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Http post: ");
            usart_println_sync(get_main_usart(), fileBuffer);
            usart_println_sync(sim868_usart, fileBuffer);
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 10:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Http action");
            usart_println_sync(sim868_usart, "AT+HTTPACTION=1");
            status = SIM868_STATUS_BUSY;
            httpStatus = SIM868_HTTP_PENDING;
            ml_status++;
            break;
        case 11:
            if (httpStatus == SIM868_HTTP_PENDING) {break;}
            usart_println_sync(get_main_usart(), "Http term");
            usart_println_sync(sim868_usart, "AT+HTTPTERM");
            status = SIM868_STATUS_BUSY;
            ml_status++;
            break;
        case 12:
            SIM868_async_wait();
            if (httpStatus != SIM868_HTTP_200) {
                usart_println_sync(get_main_usart(), "Http fail, create file");
                sim868_createFileName(today_filebuffer_index);
                today_filebuffer_index++;
                usart_print_sync(sim868_usart, "AT+FSCREATE=C:\\buffer\\today\\");
                usart_println_sync(sim868_usart, fileName);
                status = SIM868_STATUS_BUSY;
                ml_status++;
            } else {
                usart_println_sync(get_main_usart(), "Http 200, goto 3");
                ml_status = 3;
            }
            break;
        case 13:
            SIM868_async_wait();
            len = strlen(fileBuffer);
            lens[0] = 0;
            sprintf(lens, "%d", len);
            usart_println_sync(get_main_usart(), "Write file");
            usart_print_sync(sim868_usart, "AT+FSWRITE=C:\\buffer\\today\\");
            usart_print_sync(sim868_usart, fileName);
            usart_print_sync(sim868_usart, ",0,");
            usart_print_sync(sim868_usart, lens);
            usart_println_sync(sim868_usart, ",2");
            status = SIM868_STATUS_WAIT_FILEINPUT;
            ml_status++;
            break;
        case 14:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "Transmit file data");
            usart_println_sync(sim868_usart, fileBuffer);
            status = SIM868_STATUS_BUSY;
            ml_status = 3;
            break;
        case 15:
            SIM868_async_wait();
            usart_println_sync(sim868_usart, "AT+FSRMDIR=C:\\buffer\\prev\\");
            status = SIM868_STATUS_BUSY;
            ml_status = 20;
            break;

        case 20:
            if (httpBuffer != 0) {
                usart_println_sync(get_main_usart(), "Send new http post");
                uint16_t len = strlen(httpBuffer);
                memcpy(fileBuffer, httpBuffer, len);
                ml_status = 6;
            } else {
                SIM868_async_wait();
                usart_println_sync(get_main_usart(), "Check failed today");
                fileName[0] = 0;
                usart_println_sync(sim868_usart, "AT+FSLS=C:\\buffer\\today\\");
                status = SIM868_STATUS_WAIT_FILENAME;
                ml_status++;
            }
            break;
        case 21:
            SIM868_async_wait();
            if (fileName[0] != 0) {
                usart_println_sync(get_main_usart(), "Get today file content");
                usart_print_sync(sim868_usart, "AT+FSREAD=C:\\buffer\\today\\");
                usart_print_sync(sim868_usart, fileName);
                usart_println_sync(sim868_usart, ",0,255,0");
                status = SIM868_STATUS_WAIT_FILECONTENT;
                ml_status = 5;
            } else {
                usart_println_sync(get_main_usart(), "No today files, goto 20");
                ml_status = 20;
            }
            break;
    }
}

void sim868_loop(void) {
    if (imei[0] == 0 && loopStatus != SIM868_LOOP_INIT) {
        loopStatus = SIM868_LOOP_GET_IMEI;
    }

    switch (loopStatus) {
        case SIM868_LOOP_INIT:
            status = SIM868_STATUS_FREE;
            loopStatus = SIM868_LOOP_GET_IMEI;
            break;
        case SIM868_LOOP_GET_IMEI:
            SIM868_async_wait();
            usart_println_sync(get_main_usart(), "AT+GSN");
            usart_println_sync(sim868_usart, "AT+GSN");
            status = SIM868_STATUS_WAITIMEI;
//            sim868_enableGnss();
            loopStatus = SIM868_LOOP_OK;
            break;
        case SIM868_LOOP_POWER_DOWN:
            status = SIM868_STATUS_OFF;
            break;
        case SIM868_LOOP_CPIN_READY:
            imei[0] = 0;
            loopStatus = SIM868_LOOP_INIT;
            break;
        case SIM868_LOOP_OK:
            sim868_main_loop();
            break;
        default:
            break;
    }
}

void sim868_enableGnss(void) {
//    usart_println_sync(get_main_usart(), "enable gnss wait");
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+CGNSPWR=1");
    usart_println_sync(sim868_usart, "AT+CGNSPWR=1");
    status = SIM868_STATUS_BUSY;

//    usart_println_sync(get_main_usart(), "enable cgnsurc wait");
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+CGNSURC=2");
    usart_println_sync(sim868_usart, "AT+CGNSURC=2");
    status = SIM868_STATUS_BUSY;
}

void sim868_httpUrl(char* url) {
    pUrl = url;
}

void sim868_post(char* url, char *data) {
    SIM868_wait();
    usart_println_sync(get_main_usart(), "AT+HTTPINIT");
    usart_println_sync(sim868_usart, "AT+HTTPINIT");
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    usart_print_sync(get_main_usart(), "AT+HTTPPARA=URL,");
    usart_print_sync(sim868_usart, "AT+HTTPPARA=URL,");
    usart_println_sync(get_main_usart(), url);
    usart_println_sync(sim868_usart, url);
    status = SIM868_STATUS_BUSY;

    SIM868_wait();
    httpStatus = SIM868_HTTP_READY;

    uint16_t len = strlen(data);
    char lens[6] = {0};
    sprintf(lens, "%d", len);

    SIM868_wait();
    usart_print_sync(get_main_usart(), "AT+HTTPDATA=");
    usart_print_sync(sim868_usart, "AT+HTTPDATA=");
    usart_print_sync(get_main_usart(), lens);
    usart_print_sync(sim868_usart, lens);
    usart_println_sync(get_main_usart(), ",2000");
    usart_println_sync(sim868_usart, ",2000");
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait DOWNLOAD response
    usart_println_sync(sim868_usart, data);
    status = SIM868_STATUS_BUSY;

    SIM868_wait(); // Wait OK response
    usart_println_sync(get_main_usart(), "AT+HTTPACTION=1");
    usart_println_sync(sim868_usart, "AT+HTTPACTION=1");
    status = SIM868_STATUS_BUSY;
    httpStatus = SIM868_HTTP_PENDING;

    while (httpStatus == SIM868_HTTP_PENDING) {_delay_ms(1);}
    usart_print_sync(get_main_usart(), "HTTP RESULT: ");
    usart_println_sync(get_main_usart(), httpStatus == SIM868_HTTP_200 ? "200" : "FAILED");
    httpStatus = SIM868_HTTP_READY;

    usart_println_sync(get_main_usart(), "AT+HTTPTERM");
    usart_println_sync(sim868_usart, "AT+HTTPTERM");
}

void sim868_post_async(char *data) {
    httpBuffer = data;
}
