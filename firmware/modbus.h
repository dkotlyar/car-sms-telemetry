/*
 * modbus.h
 *
 * Created: 12.08.2019 15:00:00
 *  Author: emedvedev
 */ 

#include "main.h"

#define MODBUS_USART_BAUD       57600 // скорость шины usart
#define MODBUS_ADDRESS          1 // адрсес в сети modbus
#define REG_COUNT_HOLDING       1 // кол-во регистров holding
#define START_HOLDING_ADDRESS   0 // начальный адрес holding
#define REG_COUNT_INPUT         11 // кол-во регистров input
#define START_INPUT_ADDRESS     0 // начальный адрес input

volatile uint16_t holdingRegisters[REG_COUNT_HOLDING]; // регистры блока НПО
volatile uint16_t inputRegisters[REG_COUNT_INPUT]; // регистры блока НПО

/*
 * Процедура запуска modbus
 */
void modbus_init(void);
void modbus_start(void);
void modbus_stop(void);
void modbus_clear_reg(void);

/*
 * Процедура обработки modbus, следует вызывать в основом цикле программы
 */
void modbus_loop(void);


/*
 *  Процедура вызова таймеров modbus, следует вызывать в прерываниии 10kHz
 */
void modbus_tickTimer(void);

/*
 *  Функция получения времени последнего принятого сообщения
 */
uint32_t modbus_getReciveTime(void);
