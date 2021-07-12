#ifndef OBD_SCAN_OBD2_H
#define OBD_SCAN_OBD2_H

#include "main.h"
#include "canlib/can_lib.h"

#define OBD2_TX_IDE   0x7DF
#define OBD2_RX_IDE   0x7E8
#define OBD2_RX_IDMSK 0x7C0

#define OBD2_PIDS   6

#define OBD2_PARALLEL_REQUESTS  5

#define OBD2_REQUEST_NEW                0
#define OBD2_REQUEST_REGISTER_RX_MOB    1
#define OBD2_REQUEST_REGISTER_TX_MOB    2
#define OBD2_REQUEST_TX_PENDING         3
#define OBD2_REQUEST_RX_PENDING         4
#define OBD2_REQUEST_OK                 100
#define OBD2_REQUEST_TIMEOUT            254
#define OBD2_REQUEST_ERROR              255

#define OBD2_DEFAULT_TIMEOUT    2000 // in ms

#define OBD2_PID_NODATA         0
#define OBD2_PID_OK             1
#define OBD2_PID_DISABLED       2
#define OBD2_PID_REQUESTED      3

typedef struct {
    uint8_t service_number;
    uint8_t pid_code;

    uint8_t response[4];
    uint8_t status;

    uint16_t timeout;
} obd2_request_t;

typedef struct {
    uint8_t pid_code;
    uint8_t status;
    uint32_t timestamp;
    st_cmd_t can_rx;
    st_cmd_t can_tx;
} obd2_request_async_t;

typedef struct {
    uint8_t pid_code;

    uint8_t A;
    uint8_t B;
    uint8_t C;
    uint8_t D;

    uint32_t timestamp;
    uint8_t status;
} obd2_pid_t;

void obd2_init(void);
void obd2_loop(void);
void obd2_write(uint8_t service_number, uint8_t pid_code, uint8_t A, uint8_t B, uint8_t C, uint8_t D);
void obd2_abort(uint8_t pid_code);
uint8_t obd2_getnext_pid_code(void);
uint8_t obd2_request_sync(obd2_request_t *req);
void pid_request(void);

#endif //OBD_SCAN_OBD2_H
