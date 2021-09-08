/*
 * modbus.c
 *
 * Created: 12.08.2019 15:00:00
 *  Author: emedvedev
 */ 

#include "main.h"
#include "millis.h"
#include "modbus.h"
#include "yaMBSiavr.h"

modbus_t modbus_tr;

uint32_t lastSuccessReceiveTime = 0;

void modbus_start(void) {
	modbus_tr.BusState = 0;
	modbus_tr.modbusTimer = 0;
	modbus_tr.DataPos = 0;
	modbus_tr.PacketTopIndex = 7;
	modbus_tr.modBusStaMaStates = 0;
	modbus_tr.physicalType = 0;
//	modbus_tr.transEnablePort = &PORTE;
//	modbus_tr.transEnablePin = 6;
//	modbus_tr.transEnableDdr = &DDRE;
	modbus_tr.usartStatus = &UCSR1A;
	modbus_tr.usartControl = &UCSR1B;
	modbus_tr.usartData = &UDR1;
	modbus_tr.usartUdrie = UDRIE1;
	modbus_tr.ucsrc = &UCSR1C;
	modbus_tr.rxcie = RXCIE1;
	modbus_tr.txcie = TXCIE1;
	modbus_tr.rxen = RXEN1;
	modbus_tr.txen = TXEN1;
	modbus_tr.ucsz0 = UCSZ10;
	modbus_tr.ucsz1 = UCSZ11;
	modbus_tr.ubrrh = &UBRR1H;
	modbus_tr.ubrrl = &UBRR1L;
	modbus_tr.rsAddress = MODBUS_ADDRESS;
	yaMBSiavr_modbusInit(&modbus_tr, MODBUS_USART_BAUD);
}

void modbus_clear_reg(void) {
	for (uint8_t i = 0; i < REG_COUNT_HOLDING; i++) holdingRegisters[i] = 0;
	for (uint8_t i = 0; i < REG_COUNT_INPUT; i++) inputRegisters[i] = 0;
}

void modbus_stop(void) {
	*modbus_tr.usartControl = 0;
	PORTD &= ~((1<<2)|(1<<3));
	DDRD |= ((1<<2)|(1<<3));
}

uint32_t modbus_getReciveTime(void) {
	return lastSuccessReceiveTime;
}

void modbus_init(void) {
    modbus_start();
    modbus_clear_reg();
}

ISR (USART1_TX_vect) {
	yaMBSiavr_usartTransmitCompleteInterrupt(&modbus_tr);
}

ISR (USART1_RX_vect) {
	yaMBSiavr_usartReviceInterrupt(&modbus_tr);
}

ISR (USART1_UDRE_vect) {
	yaMBSiavr_usartTransmitInterrupt(&modbus_tr);
}

void modbus_getTr(void) {
	if (yaMBSiavr_modbusGetBusState(&modbus_tr) & (1 << ReceiveCompleted))
	{
		switch(yaMBSiavr_getModbusCommand(&modbus_tr)) {
			//Если команда чтения одного HoldingRegisters
			case fcReadHoldingRegisters: {
				lastSuccessReceiveTime = millis();
				yaMBSiavr_modbusExchangeRegisters(&modbus_tr, holdingRegisters ,START_HOLDING_ADDRESS, REG_COUNT_HOLDING);
			}
			break;
			//Если команда чтения одного InputRegisters
			case fcReadInputRegisters: {
				lastSuccessReceiveTime = millis();
				yaMBSiavr_modbusExchangeRegisters(&modbus_tr, inputRegisters ,START_INPUT_ADDRESS, REG_COUNT_INPUT);
			}
			break;
			//Если команда записи одного HoldingRegisters
			case fcPresetSingleRegister: {
				lastSuccessReceiveTime = millis();
				yaMBSiavr_modbusExchangeRegisters(&modbus_tr, holdingRegisters ,START_HOLDING_ADDRESS, REG_COUNT_HOLDING);
			}
			break;
			//Если команда записи множетсва HoldingRegisters
			case fcPresetMultipleRegisters: {
				lastSuccessReceiveTime = millis();
				yaMBSiavr_modbusExchangeRegisters(&modbus_tr, holdingRegisters ,START_HOLDING_ADDRESS, REG_COUNT_HOLDING);
			}
			break;
			//Если иное, то передаем ошибку по ModBus
			default: {
				yaMBSiavr_modbusSendExeption(&modbus_tr, ecIllegalFunction);
			}
			break;
		}
		
	}
}

void modbus_loop(void) {
	modbus_getTr();
}

void modbus_tickTimer(void) {
	yaMBSiavr_modbusTickTimer(&modbus_tr);
}
