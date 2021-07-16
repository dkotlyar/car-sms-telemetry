#include "obd2.h"
#include "main.h"
#include "canlib/can_lib.h"
#include "usart_lib.h"
#include "millis.h"

uint8_t engine_coolant_temperature; // PID 05
uint8_t vehicle_speed; // PID 0D
uint16_t run_time_since_engine_start; // PID 1F
uint8_t fuel_level; // PID 2F
uint32_t odometr; // PID A6

st_cmd_t can_rx;
st_cmd_t can_tx;
uint8_t obd_rx_buffer[8];
uint8_t obd_tx_buffer[8];

void obd2_init(void) {
    can_init(0);
    can_rx.pt_data = &obd_rx_buffer[0];
    can_tx.pt_data = &obd_tx_buffer[0];
}

void obd2_handle(uint8_t* pt_data) {
    usart_println_sync(get_main_usart(), "handle: ");
    for (uint8_t i = 0; i < 8; i++) {
        usart_send_sync(get_main_usart(), pt_data[i]);
    }
    return;

//    uint8_t service_number = pt_data[1];
//    if (service_number != 0x41) {
//        return;
//    }
//
//    uint8_t pid_code = pt_data[2];
//    uint8_t *payload = &pt_data[3];
//
//    switch (pid_code) {
//        case 05: // Engine coolant temperature
//            engine_coolant_temperature = payload[0] - 40;
//            break;
//    }
}

uint8_t obd2_reqloop(uint8_t pid_code) {
    uint8_t rx_status;
    uint8_t tx_status;

    static uint8_t status = 0;
    static uint32_t watchdog = 0;

    uint8_t retval = 0;

    switch (status) {
        case 0:
            status++;
            break;
        case 1:
            can_rx.status = 0;
            can_rx.id.std = OBD2_RX_IDE;
            can_rx.idmsk.std = OBD2_RX_IDMSK;
            can_rx.ctrl.ide = 0;
            can_rx.ctrl.rtr = 0;
            can_rx.dlc = 8;
            can_rx.cmd = CMD_RX_DATA_MASKED;
            for (uint8_t j = 0; j < 8; j++) { can_tx.pt_data[j] = 0; }
            can_tx.pt_data[0] = 2; // Количество байт (всегда 2)
            can_tx.pt_data[1] = 1; // ID сервиса (всегда 1)
            can_tx.pt_data[2] = pid_code;
            can_tx.status = 0;
            can_tx.id.std = OBD2_TX_IDE;
            can_tx.ctrl.ide = 0;
            can_tx.ctrl.rtr = 0;
            can_tx.dlc = 8;
            can_tx.cmd = CMD_TX_DATA;
            status++;
            break;
        case 2:
            if (can_cmd(&can_rx) == CAN_CMD_ACCEPTED) {
                status++;
                usart_println_sync(get_main_usart(), "rx registred");
            }
            break;
        case 3:
            if (can_cmd(&can_tx) == CAN_CMD_ACCEPTED) {
                status++;
                watchdog = millis();
                usart_println_sync(get_main_usart(), "tx registred");
            }
            break;
        case 4:
            tx_status = can_get_status(&can_tx);
            if (tx_status == CAN_STATUS_COMPLETED) {
                status++;
                watchdog = millis();
                usart_println_sync(get_main_usart(), "tx pushed");
            } else if (tx_status == CAN_STATUS_ERROR) {
                status = OBD2_REQUEST_ERROR;
                usart_println_sync(get_main_usart(), "tx err");
            }
            if ((millis() - watchdog) > OBD2_DEFAULT_TIMEOUT) {
                status = OBD2_REQUEST_TIMEOUT;
                usart_println_sync(get_main_usart(), "tx timeout");
            }
            break;
        case 5:
            rx_status = can_get_status(&can_rx);
            if (rx_status == CAN_STATUS_COMPLETED) {
                usart_println_sync(get_main_usart(), "rx received");
                obd2_handle(can_rx.pt_data);
                status = OBD2_REQUEST_OK;
            } else if (rx_status == CAN_STATUS_ERROR) {
                status = OBD2_REQUEST_ERROR;
                usart_println_sync(get_main_usart(), "rx err");
            }
            if ((millis() - watchdog) > OBD2_DEFAULT_TIMEOUT) {
                status = OBD2_REQUEST_TIMEOUT;
                usart_println_sync(get_main_usart(), "rx timeout");
            }
            break;
        case OBD2_REQUEST_OK:
            status = 0;
            retval = 1;
            break;
        case OBD2_REQUEST_TIMEOUT:
        case OBD2_REQUEST_ERROR:
            can_rx.cmd = CMD_ABORT;
            can_tx.cmd = CMD_ABORT;
            can_cmd(&can_rx);
            can_cmd(&can_tx);
            status = 0;
            retval = 1;
            break;
        default:
            status = OBD2_REQUEST_ERROR;
            break;
    }

    return retval;
}

void obd2_loop(void) {
    uint8_t val = obd2_reqloop(05);
}

uint8_t obd2_request_sync(uint8_t service_number, uint8_t pid_code) {
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
    tx_remote_buffer[1] = service_number;
    tx_remote_buffer[2] = pid_code;

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
    for (uint16_t i = 0; i < OBD2_DEFAULT_TIMEOUT && (response_status = can_get_status(&response_msg)) == CAN_STATUS_NOT_COMPLETED; i++) {
        _delay_ms(1);
    }

    // If response is received
    if (response_status == CAN_STATUS_NOT_COMPLETED) {
        return OBD2_REQUEST_TIMEOUT;
    } else if (response_status == CAN_STATUS_COMPLETED) {
        obd2_handle(response_msg.pt_data);
        return OBD2_REQUEST_OK;
    } else {
        // If no response is received in 50ms, send abort-frame
        response_msg.cmd = CMD_ABORT;
        while (can_cmd(&response_msg) != CAN_CMD_ACCEPTED);
        return OBD2_REQUEST_ERROR;
    }
}

//void pid_request(void) {
//    obd2_request_t req;
//    req.service_number = 1;
//    req.pid_code = 0;
//    req.status = OBD2_REQUEST_NEW;
//
//    uint8_t res = obd2_request_sync(&req);
//
//    usart_send_sync(&usart0, res);
//}