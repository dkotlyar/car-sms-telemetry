/*************************************************************************
Title:    Yet another (small) modbus (server) implementation for the avr.
Author:   Max Brueggemann
Hardware: any AVR with hardware UART, tested on Atmega 88/168 at 20Mhz
License:  BSD-3-Clause
          
DESCRIPTION:
    Refer to the header file yaMBSiavr.h.
    
USAGE:
    Refer to the header file yaMBSiavr.h.
                    
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
                        
*************************************************************************/

#include <avr/io.h>
#include "yaMBSiavr.h"
#include <avr/interrupt.h>

uint8_t yaMBSiavr_modbusGetBusState(modbus_t * modbus) {
	return modbus->BusState;
}

void transceiver_txen(modbus_t * modbus) {
	*modbus->transEnablePort |= (1 << modbus->transEnablePin);
}

void transceiver_rxen(modbus_t * modbus) {
    *modbus->transEnablePort &= ~(1 << modbus->transEnablePin);
}
 
uint8_t crc16(volatile uint8_t *ptrToArray,uint8_t inputSize) //A standard CRC algorithm
{
	uint16_t out=0xffff;
	uint16_t carry;
	unsigned char n;
	inputSize++;
	for (int l=0; l<inputSize; l++) {
		out ^= ptrToArray[l];
		for (n = 0; n < 8; n++) {
			carry = out & 1;
			out >>= 1;
			if (carry) out ^= 0xA001;
		}
	}
	if ((ptrToArray[inputSize]==out%256) && (ptrToArray[inputSize+1]==out/256)) //check
	{
		return 1;
	} else { 
		ptrToArray[inputSize]=out%256; //append Lo
		ptrToArray[inputSize+1]=out/256; //append Hi
		return 0;	
	}
}

/* @brief: copies a single or multiple words from one array of bytes to another array of bytes
*          amount must not be bigger than 255...
*
*/

void listRegisterCopy(volatile uint8_t *source, volatile uint8_t *target, uint8_t amount)
{
	for (uint8_t c=0; c<amount; c++)
	{
		*(target+c)=*(source+c);
	}
}

/* @brief: copies a single bit from one char to another char (or arrays thereof)
*
*
*/

/* @brief: Back to receiving state.
*
*/

void modbusReset(modbus_t * modbus) {
	modbus->BusState = (1 << TimerActive);
	modbus->modbusTimer = 0;
}

void yaMBSiavr_modbusTickTimer(modbus_t * modbus) {
	if (modbus->BusState & (1 << TimerActive))
	{
		modbus->modbusTimer++;
		if (modbus->BusState & (1 << Receiving))
		{
			if (modbus->modbusTimer == modbusInterCharTimeout)
			{
				modbus->BusState |= (1 << GapDetected);
			} else if (modbus->modbusTimer == modbusInterFrameDelayReceiveEnd)
			{
				modbus->BusState = (1 << ReceiveCompleted);
				if (modbus->mbBuffer[0] == modbus->rsAddress && crc16(modbus->mbBuffer, modbus->DataPos -3))
				{
					
				} else modbusReset(modbus);	
			}
		} else if (modbus->modbusTimer == modbusInterFrameDelayReceiveStart) modbus->BusState |= (1 << BusTimedOut);
	}
}

void yaMBSiavr_usartReviceInterrupt(modbus_t * modbus) {
	unsigned char data;
	data = *modbus->usartData;
	modbus->modbusTimer = 0;
	if (!(modbus->BusState & (1 << ReceiveCompleted)) && !(modbus->BusState & (1 <<TransmitRequested)) && !(modbus->BusState & (1 <<Transmitting))
		&&  (modbus->BusState & (1 << Receiving)) && !(modbus->BusState & (1 << BusTimedOut))) {
		if (modbus->DataPos > MaxFrameIndex) modbusReset(modbus);
		else {
			modbus->mbBuffer[modbus->DataPos] = data;
			modbus->DataPos++;
		}
		
	} else if (!(modbus->BusState & (1<<ReceiveCompleted)) && !(modbus->BusState & (1<<TransmitRequested)) && !(modbus->BusState & (1<<Transmitting))
		&& !(modbus->BusState & (1<<Receiving)) && (modbus->BusState & (1<<BusTimedOut)))
	{
		modbus->mbBuffer[0] = data;
		modbus->BusState = ((1 << Receiving) | (1 << TimerActive));
		modbus->DataPos = 1;
	}
}

void yaMBSiavr_usartTransmitInterrupt(modbus_t * modbus) {
	modbus->BusState &= ~(1 << TransmitRequested);
	modbus->BusState |= (1 << Transmitting);
	*modbus->usartData = modbus->mbBuffer[modbus->DataPos];
	modbus->DataPos++;
	if (modbus->DataPos == (modbus->PacketTopIndex +1))
	{
		*modbus->usartControl &= ~(1 << modbus->usartUdrie);
	}
	
}

void yaMBSiavr_usartTransmitCompleteInterrupt(modbus_t * modbus) {
	if (modbus->physicalType == 485)
	{
		transceiver_rxen(modbus);
	}
	 modbusReset(modbus);
}


void yaMBSiavr_modbusInit(modbus_t * modbus, uint16_t baud) {
	uint16_t speed = (F_CPU / 16 / baud) - 1;
	*modbus->ubrrh = (unsigned char)((speed) >> 8);
	*modbus->ubrrl = (unsigned char) speed;
	*modbus->usartStatus = 0;
	*modbus->usartControl = (1 << modbus->txcie)|(1 << modbus->rxcie)|(1 << modbus->rxen)|(1 << modbus->txen);
	*modbus->ucsrc = (1 << modbus->ucsz1)|(1 << modbus->ucsz0);
	if (modbus->physicalType == 485)
	{
		*modbus->transEnableDdr |= ( 1 << modbus->transEnablePin);
	}
	transceiver_rxen(modbus);
	modbus->BusState = (1 << TimerActive);
}

void modbusSendMessage(modbus_t * modbus, unsigned char packtop) {
	modbus->PacketTopIndex = packtop + 2;
	crc16(modbus->mbBuffer, packtop);
	modbus->BusState |= (1 << TransmitRequested);
	modbus->DataPos = 0;
	if (modbus->physicalType == 485)
	{
		transceiver_txen(modbus);
	}
	*modbus->usartControl |= (1 << modbus->usartUdrie);
	modbus->BusState &= ~(1 << ReceiveCompleted);
}

void yaMBSiavr_modbusSendExeption(modbus_t * modbus, unsigned char exceptionCode) {
	modbus->mbBuffer[1] |= (1 << 7);
	modbus->mbBuffer[2] = exceptionCode;
	modbusSendMessage(modbus, 2);
}

uint16_t modbusRequestedAmount(modbus_t * modbus)
{
	return (modbus->mbBuffer[5] | (modbus->mbBuffer[4] << 8));
}

uint16_t modbusRequestedAddress(modbus_t * modbus)
{
	return (modbus->mbBuffer[3] | (modbus->mbBuffer[2] << 8));
}

uint8_t yaMBSiavr_getModbusAddress(modbus_t * modbus) {
	return modbus->mbBuffer[0];
}

uint8_t yaMBSiavr_getModbusCommand(modbus_t * modbus) {
	return modbus->mbBuffer[1];
}

void modbusRegisterToInt(volatile uint8_t *inreg, volatile uint16_t *outreg, uint8_t amount)
{
	for (uint8_t c=0; c<amount; c++)
	{
		*(outreg+c) = (*(inreg+c*2) << 8) + *(inreg+1+c*2);
	}
}

void intToModbusRegister(volatile uint16_t *inreg, volatile uint8_t *outreg, uint8_t amount)
{
	for (uint8_t c=0; c<amount; c++)
	{
		*(outreg+c*2) = (uint8_t)(*(inreg+c) >> 8);
		*(outreg+1+c*2) = (uint8_t)(*(inreg+c));
	}
}

uint8_t yaMBSiavr_modbusExchangeRegisters(modbus_t * modbus, volatile uint16_t *ptrToInArray,  uint16_t startAddress, uint16_t size) {
	uint16_t requestedAmount = modbusRequestedAmount(modbus);
	uint16_t requestedAdr = modbusRequestedAddress(modbus);
	if (modbus->mbBuffer[1] == fcPresetSingleRegister) requestedAmount = 1;
	if ((requestedAdr >= startAddress) && ((startAddress + size) >= (requestedAmount + requestedAdr))) {
		
		if ((modbus->mbBuffer[1] == fcReadHoldingRegisters) || (modbus->mbBuffer[1] == fcReadInputRegisters) )
		{
			if ((requestedAmount * 2) <= (MaxFrameIndex - 4)) //message buffer big enough?
			{
				modbus->mbBuffer[2] = (unsigned char)(requestedAmount * 2);
				intToModbusRegister(ptrToInArray + (unsigned char)(requestedAdr - startAddress),modbus->mbBuffer + 3,requestedAmount);
				modbusSendMessage(modbus, 2 + modbus->mbBuffer[2]);
				return 1;
			} else yaMBSiavr_modbusSendExeption(modbus, ecIllegalDataValue);
		}
		else if (modbus->mbBuffer[1] == fcPresetMultipleRegisters)
		{
			if (((modbus->mbBuffer[6]) >= requestedAmount * 2) && ((modbus->DataPos-9) >= modbus->mbBuffer[6])) //enough data received?
			{
				modbusRegisterToInt(modbus->mbBuffer + 7,ptrToInArray + (unsigned char)(requestedAdr - startAddress),(unsigned char)(requestedAmount));
				modbusSendMessage(modbus, 5);
				return 1;
			} else yaMBSiavr_modbusSendExeption(modbus, ecIllegalDataValue);//too few data bytes received
		}
		else if (modbus->mbBuffer[1] == fcPresetSingleRegister)
		{
			modbusRegisterToInt(modbus->mbBuffer + 4,ptrToInArray + (unsigned char)(requestedAdr - startAddress), 1);
			modbusSendMessage(modbus, 5);
			return 1;
		}
		//modbusSendException(ecSlaveDeviceFailure); //inapropriate call of modbusExchangeRegisters
		return 0;
		} else {
			yaMBSiavr_modbusSendExeption(modbus, ecIllegalDataValue);
		return 0;
	}
	
}
