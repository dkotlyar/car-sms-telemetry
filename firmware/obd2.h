#ifndef OBD_SCAN_OBD2_H
#define OBD_SCAN_OBD2_H

#include "main.h"
#include "canlib/can_lib.h"

#define OBD2_TX_IDE   0x7DF
#define OBD2_RX_IDE   0x7E8
#define OBD2_RX_IDMSK 0x7C0

#define OBD2_REQUEST_OK         100
#define OBD2_REQUEST_TIMEOUT    254
#define OBD2_REQUEST_ERROR      255

#define OBD2_DEFAULT_TIMEOUT    2000 // in ms

void obd2_init(void);
void obd2_loop(void);
//void obd2_write(uint8_t service_number, uint8_t pid_code, uint8_t A, uint8_t B, uint8_t C, uint8_t D);
//void obd2_abort(uint8_t pid_code);
uint8_t obd2_request_sync(uint8_t service_number, uint8_t pid_code);
//void pid_request(void);

#endif //OBD_SCAN_OBD2_H
