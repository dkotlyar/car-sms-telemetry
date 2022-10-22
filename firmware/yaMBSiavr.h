#ifndef yaMBIavr_H
#define yaMBIavr_H
#endif
/************************************************************************
Title:    Yet another (small) Modbus (server) implementation for the avr.
Author:   Max Brueggemann
Hardware: any AVR with hardware UART, tested on Atmega 88/168 at 20Mhz
License:  BSD-3-Clause

LICENSE:

Copyright 2017 Max Brueggemann, www.maxbrueggemann.de

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
    
************************************************************************/

/** 
 *  @code #include <yaMBSIavr.h> @endcode
 * 
 *  @brief   Interrupt-based Modbus implementation for small avr microcontrollers.
 *           The Modbus implementation guidelines at modbus.org call for response
 *           timeouts in the range of several seconds , hence only timing critical
 *           parts have been implemented within ISRs. The actual handling of the Modbus
 *           frame can easily be done in the main while loop.
 *
 *  @author Max Brueggemann www.maxbrueggemann.de
 */

#include "main.h"

#define modbusInterFrameDelayReceiveStart 16
#define modbusInterFrameDelayReceiveEnd 18
#define modbusInterCharTimeout 7

/**
 * @brief    Defines the maximum Modbus frame size accepted by the device. 255 is the default
 *           and also the maximum value. However, it might be useful to set this to lower
 *           values, with 8 being the lowest possible value, in order to save on ram space.
 */
#define MaxFrameIndex 255

/**
 * @brief    Modbus Function Codes
 *           Refer to modbus.org for further information.
 *           It's good practice to return exception code 01 in case you receive a function code
 *           that you haven't implemented in your application.
 */
#define fcReadCoilStatus 1 //read single/multiple coils
#define fcReadInputStatus 2 //read single/multiple inputs
#define fcReadHoldingRegisters 3 //read analog output registers *
#define fcReadInputRegisters 4 //read analog input registers (2 Bytes per register) *
#define fcForceSingleCoil 5 //write single bit
#define fcPresetSingleRegister 6 //write analog output register (2 Bytes)
#define fcForceMultipleCoils 15 //write multiple bits
#define fcPresetMultipleRegisters 16 //write multiple analog output registers (2 Bytes each) *
#define fcReportSlaveID 17 //read device description, run status and other device specific information

/**
 * @brief    Modbus Exception Codes
 *           Refer to modbus.org for further information.
 *           It's good practice to return exception code 01 in case you receive a function code
 *           that you haven't implemented in your application.
 */
#define ecIllegalFunction 1 
#define ecIllegalDataAddress 2 
#define ecIllegalDataValue 3
#define ecSlaveDeviceFailure 4
#define ecAcknowledge 5
#define ecSlaveDeviceBusy 6
#define ecNegativeAcknowledge 7
#define ecMemoryParityError 8

/**
 * @brief    Internal bit definitions
 */
#define BusTimedOut 0
#define Receiving 1
#define Transmitting 2
#define ReceiveCompleted 3
#define TransmitRequested 4
#define TimerActive 5
#define GapDetected 6

typedef struct __modbus__ {
	volatile uint8_t rsAddress;
	volatile unsigned char BusState;
	volatile uint16_t modbusTimer;
	volatile uint16_t DataPos;
	volatile unsigned char PacketTopIndex;
	volatile unsigned char modBusStaMaStates;
	
	unsigned char mbBuffer[MaxFrameIndex+1];
	
	uint16_t physicalType; // 232/485
	
	//Пины управления направлением передачи
	volatile uint8_t * transEnablePort;
	volatile uint8_t * transEnableDdr;
	uint8_t transEnablePin;
	
	volatile uint8_t * usartControl;
	volatile uint8_t * usartData;
	volatile uint8_t usartUdrie;
	volatile uint8_t * usartStatus;
	
	volatile uint8_t * ubrrh;
	volatile uint8_t * ubrrl;
	
	volatile uint8_t txcie;
	volatile uint8_t rxcie;
	volatile uint8_t txen;
	volatile uint8_t rxen;
	
	volatile uint8_t ucsz0;
	volatile uint8_t ucsz1;
	
	volatile uint8_t * ucsrc;
} modbus_t;

void yaMBSiavr_modbusInit(modbus_t * modbus, uint16_t baud);
uint8_t yaMBSiavr_modbusGetBusState(modbus_t * modbus);
void yaMBSiavr_modbusTickTimer(modbus_t * modbus);
void yaMBSiavr_usartReviceInterrupt(modbus_t * modbus);
void yaMBSiavr_usartTransmitInterrupt(modbus_t * modbus);
void yaMBSiavr_usartTransmitCompleteInterrupt(modbus_t * modbus);
uint8_t yaMBSiavr_modbusExchangeRegisters(modbus_t * modbus, volatile uint16_t *ptrToInArray,  uint16_t startAddress, uint16_t size);
uint8_t yaMBSiavr_getModbusAddress(modbus_t * modbus);
uint8_t yaMBSiavr_getModbusCommand(modbus_t * modbus);
void yaMBSiavr_modbusSendExeption(modbus_t * modbus, unsigned char exceptionCode);
