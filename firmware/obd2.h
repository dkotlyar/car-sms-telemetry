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

#define OBD2_PID_ENGINE_COOLANT_TEMPERATURE     0x05
#define OBD2_PID_ENGINE_SPEED                   0x0C
#define OBD2_PID_VEHICLE_SPEED                  0x0D
#define OBD2_PID_RUN_TIME_SINCE_ENGINE_START    0x1F
#define OBD2_PID_FUEL_LEVEL                     0x2F
#define OBD2_PID_ODOMETER                       0xA6
#define OBD2_PID_DISTANCE_TRAVELED_SINCE_CODES_CLEARED 0x31

extern int8_t engine_coolant_temperature;
extern uint16_t engine_speed;
extern uint8_t vehicle_speed;
extern uint16_t run_time_since_engine_start;
extern uint8_t fuel_level;
extern uint32_t odometer;
extern uint16_t distance_traveled_since_codes_cleared;
extern uint32_t timestamp;

void obd2_init(void);
void obd2_loop(void);
//void obd2_write(uint8_t service_number, uint8_t pid_code, uint8_t A, uint8_t B, uint8_t C, uint8_t D);
//void obd2_abort(uint8_t pid_code);
uint8_t obd2_request_sync(uint8_t service_number, uint8_t pid_code);
//void pid_request(void);

uint16_t obd2_get_aprox_distance_traveled(void);
uint16_t obd2_get_runtime_since_engine_start(void);

#endif //OBD_SCAN_OBD2_H
