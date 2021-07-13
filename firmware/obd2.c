#include "obd2.h"
#include "main.h"
#include "canlib/can_lib.h"
#include "usart_lib.h"
#include "millis.h"

obd2_pid_t engine_coolant_temperature; // PID 05
obd2_pid_t engine_speed; // PID 0C
obd2_pid_t vehicle_speed; // PID 0D
obd2_pid_t run_time_since_engine_start; // PID 1F
obd2_pid_t fuel_level; // PID 2F
obd2_pid_t ambient_air_temperature; // PID 46

obd2_pid_t *pids[OBD2_PIDS];

obd2_request_async_t obd_reqs[OBD2_PARALLEL_REQUESTS];
uint8_t obd_rx_buffer[OBD2_PARALLEL_REQUESTS][8];
uint8_t obd_tx_buffer[OBD2_PARALLEL_REQUESTS][8];

void obd2_init(void) {
    engine_coolant_temperature.pid_code = 0x05;
    engine_coolant_temperature.status   = OBD2_PID_NODATA;
    pids[0] = &engine_coolant_temperature;

    engine_speed.pid_code               = 0x0C;
    engine_speed.status                 = OBD2_PID_NODATA;
    pids[1] = &engine_speed;

    vehicle_speed.pid_code              = 0x0D;
    vehicle_speed.status                = OBD2_PID_NODATA;
    pids[2] = &vehicle_speed;

    run_time_since_engine_start.pid_code = 0x1F;
    run_time_since_engine_start.status   = OBD2_PID_NODATA;
    pids[3] = &run_time_since_engine_start;

    fuel_level.pid_code                 = 0x2F;
    fuel_level.status                   = OBD2_PID_NODATA;
    pids[4] = &fuel_level;

    ambient_air_temperature.pid_code    = 0x46;
    ambient_air_temperature.status      = OBD2_PID_NODATA;
    pids[5] = &ambient_air_temperature;

    for (uint8_t i = 0; i < OBD2_PARALLEL_REQUESTS; i++) {
        obd_reqs[i].can_rx.pt_data = &obd_rx_buffer[i][0];
        obd_reqs[i].can_tx.pt_data = &obd_tx_buffer[i][0];
    }
}

void obd2_loop(void) {
    uint8_t rx_status;
    uint8_t tx_status;

    for (uint8_t i = 0; i < OBD2_PARALLEL_REQUESTS; i++) {
        switch (obd_reqs[i].status) {
            case OBD2_REQUEST_NEW:
                obd_reqs[i].pid_code = obd2_getnext_pid_code();
                if (obd_reqs[i].pid_code != 0) {
                    obd_reqs[i].can_rx.status = 0;
                    obd_reqs[i].can_rx.id.std = OBD2_RX_IDE;
                    obd_reqs[i].can_rx.idmsk.std = OBD2_RX_IDMSK;
                    obd_reqs[i].can_rx.ctrl.ide = 0;
                    obd_reqs[i].can_rx.ctrl.rtr = 0;
                    obd_reqs[i].can_rx.dlc = 8;
                    obd_reqs[i].can_rx.cmd = CMD_RX_DATA_MASKED;
                    for (uint8_t j = 0; j < 8; j++) { obd_reqs[i].can_tx.pt_data[j] = 0; }
                    obd_reqs[i].can_tx.pt_data[0] = 2; // Количество байт (всегда 2)
                    obd_reqs[i].can_tx.pt_data[1] = 1; // ID сервиса (всегда 1)
                    obd_reqs[i].can_tx.pt_data[2] = obd_reqs[i].pid_code;
                    obd_reqs[i].can_tx.status = 0;
                    obd_reqs[i].can_tx.id.std = OBD2_TX_IDE;
                    obd_reqs[i].can_tx.ctrl.ide = 0;
                    obd_reqs[i].can_tx.ctrl.rtr = 0;
                    obd_reqs[i].can_tx.dlc = 8;
                    obd_reqs[i].can_tx.cmd = CMD_TX_DATA;
                    obd_reqs[i].status = OBD2_REQUEST_REGISTER_RX_MOB;
                }
                break;
            case OBD2_REQUEST_REGISTER_RX_MOB:
                if (can_cmd(&obd_reqs[i].can_rx) == CAN_CMD_ACCEPTED) {
                    obd_reqs[i].status = OBD2_REQUEST_REGISTER_TX_MOB;
                }
                break;
            case OBD2_REQUEST_REGISTER_TX_MOB:
                if (can_cmd(&obd_reqs[i].can_tx) == CAN_CMD_ACCEPTED) {
                    obd_reqs[i].status = OBD2_REQUEST_TX_PENDING;
                    obd_reqs[i].timestamp = millis();
                }
                break;
            case OBD2_REQUEST_TX_PENDING:
                tx_status = can_get_status(&obd_reqs[i].can_tx);
                if (tx_status == CAN_STATUS_COMPLETED) {
                    obd_reqs[i].status = OBD2_REQUEST_RX_PENDING;
                    obd_reqs[i].timestamp = millis();
                } else if (tx_status == CAN_STATUS_ERROR) {
                    obd_reqs[i].status = OBD2_REQUEST_ERROR;
                }
                if ((millis() - obd_reqs[i].timestamp) > OBD2_DEFAULT_TIMEOUT) {
                    obd_reqs[i].status = OBD2_REQUEST_TIMEOUT;
                }
                break;
            case OBD2_REQUEST_RX_PENDING:
                rx_status = can_get_status(&obd_reqs[i].can_rx);
                if (rx_status == CAN_STATUS_COMPLETED) {
                    obd2_write(obd_reqs[i].can_rx.pt_data[1], obd_reqs[i].can_rx.pt_data[2],
                               obd_reqs[i].can_rx.pt_data[3], obd_reqs[i].can_rx.pt_data[4],
                               obd_reqs[i].can_rx.pt_data[5], obd_reqs[i].can_rx.pt_data[6]);
                    obd_reqs[i].status = OBD2_REQUEST_OK;
                } else {
                    obd_reqs[i].status = OBD2_REQUEST_ERROR;
                }
                if ((millis() - obd_reqs[i].timestamp) > OBD2_DEFAULT_TIMEOUT) {
                    obd_reqs[i].status = OBD2_REQUEST_TIMEOUT;
                }
                break;
            case OBD2_REQUEST_OK:
                obd_reqs[i].status = OBD2_REQUEST_NEW;
                break;
            case OBD2_REQUEST_TIMEOUT:
            case OBD2_REQUEST_ERROR:
                obd_reqs[i].can_rx.cmd = CMD_ABORT;
                obd_reqs[i].can_tx.cmd = CMD_ABORT;
                can_cmd(&obd_reqs[i].can_rx);
                can_cmd(&obd_reqs[i].can_tx);
                obd_reqs[i].status = OBD2_REQUEST_OK;
                obd2_abort(obd_reqs[i].pid_code);
                break;
            default:
                obd_reqs[i].status = OBD2_REQUEST_ERROR;
                break;
        }
    }
}

void obd2_write(uint8_t service_number, uint8_t pid_code, uint8_t A, uint8_t B, uint8_t C, uint8_t D) {
    if (service_number != 0x41) {
        return;
    }

    for (uint8_t i = 0; i < OBD2_PIDS; i++) {
        if (pids[i]->pid_code == pid_code) {
            pids[i]->A = A;
            pids[i]->B = B;
            pids[i]->C = C;
            pids[i]->D = D;
            pids[i]->timestamp = millis();
            pids[i]->status = OBD2_PID_OK;
            return;
        }
    }
}

void obd2_abort(uint8_t pid_code) {
    for (uint8_t i = 0; i < OBD2_PIDS; i++) {
        if (pids[i]->pid_code == pid_code) {
            pids[i]->status = OBD2_PID_OK;
            return;
        }
    }
}

uint8_t obd2_getnext_pid_code(void) {
    for (uint8_t i = 0; i < OBD2_PIDS; i++) {
        if (pids[i]->status == OBD2_PID_NODATA || pids[i]->status == OBD2_PID_OK) {
            return pids[i]->pid_code;
        }
    }
    return 0;
}

uint8_t obd2_request_sync(obd2_request_t *req) {
    if (req->timeout == 0) {
        req->timeout = OBD2_DEFAULT_TIMEOUT;
    }

    uint8_t response_buffer[8];
    st_cmd_t response_msg;
    response_msg.status = 0;
    response_msg.pt_data = &response_buffer[0];

    uint8_t tx_remote_buffer[8];
    st_cmd_t tx_remote_msg;
    tx_remote_msg.status = 0;
    tx_remote_msg.pt_data = &tx_remote_buffer[0];

    for (uint8_t i = 0; i < 8; i++) { tx_remote_buffer[i] = 0; }
    tx_remote_buffer[0] = 2;
    tx_remote_buffer[1] = req->service_number;
    tx_remote_buffer[2] = req->pid_code;

    response_msg.id.std = OBD2_RX_IDE; // This message object only accepts frames from Target IDs (0x80) to (0x80 + NB_TARGET)
    response_msg.idmsk.std = OBD2_RX_IDMSK; // This message object only accepts frames from Target IDs (0x80) to (0x80 + NB_TARGET)
    response_msg.ctrl.ide = 0; // This message object accepts only standard (2.0A) CAN frames
    response_msg.ctrl.rtr = 0; // this message object is not requesting a remote node to transmit data back
    response_msg.dlc = 8; // Number of bytes (8 max) of data to expect
    response_msg.cmd = CMD_RX_DATA_MASKED; // assign this as a "Receive Standard (2.0A) CAN frame" message object

    while(can_cmd(&response_msg) != CAN_CMD_ACCEPTED); // Wait for MOb to configure (Must re-configure MOb for every transaction)

    tx_remote_msg.id.std = OBD2_TX_IDE; // This message object only sends frames to Target IDs (0x80) to (0x80 + NB_TARGET)
    tx_remote_msg.ctrl.ide = 0; // This message object sends standard (2.0A) CAN frames
    tx_remote_msg.ctrl.rtr = 1; // This message object is requesting a remote node to transmit data back
    tx_remote_msg.dlc = 8; // Number of data bytes (8 max) requested from remote node
    tx_remote_msg.cmd = CMD_TX_DATA; // assign this as a "Request Standard (2.0A) Remote Data Frame" message object

    while(can_cmd(&tx_remote_msg) != CAN_CMD_ACCEPTED); // Wait for MOb to configure (Must re-configure MOb for every transaction) and send request

    while(can_get_status(&tx_remote_msg) == CAN_STATUS_NOT_COMPLETED); // Wait for Tx to complete

    uint8_t response_status;
    for (uint16_t i = 0; i < req->timeout && (response_status = can_get_status(&response_msg)) == CAN_STATUS_NOT_COMPLETED; i++) {
        _delay_ms(1);
    }

    // If response is received
    if (response_status == CAN_STATUS_NOT_COMPLETED) {
        return OBD2_REQUEST_TIMEOUT;
    } else if (response_status == CAN_STATUS_COMPLETED) {
        uint8_t service_number = response_msg.pt_data[1];
        uint8_t pid_code = response_msg.pt_data[2];
        if (service_number == req->service_number + 0x40 && pid_code == req->pid_code) {
            req->response[0] = response_msg.pt_data[3];
            req->response[1] = response_msg.pt_data[4];
            req->response[2] = response_msg.pt_data[5];
            req->response[3] = response_msg.pt_data[6];
        }
        return OBD2_REQUEST_OK;
    } else {
        // If no response is received in 50ms, send abort-frame
        response_msg.cmd = CMD_ABORT;
        while (can_cmd(&response_msg) != CAN_CMD_ACCEPTED);
        return OBD2_REQUEST_ERROR;
    }
}

void pid_request(void) {
    obd2_request_t req;
    req.service_number = 1;
    req.pid_code = 0;
    req.status = OBD2_REQUEST_NEW;

    uint8_t res = obd2_request_sync(&req);

    usart_send_sync(&usart0, res);
}