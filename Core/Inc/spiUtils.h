/*
 * spiUtils.h
 *
 *  Created on: Aug 6, 2022
 *      Author: sebas
 */

#ifndef INC_SPIUTILS_H_
#define INC_SPIUTILS_H_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define SPI_BUFF_SIZE 64
// The number of read + write attempts we will do to verify the integrity of the data written
#define NUM_DATA_CHECKS 3

// REGISTERS
#define R_RX_STATUS 0x01 - 1
#define R_TX_STATUS 0x03 - 1

#define R_RX_INTERRUPT_ENABLE 0x04

#define R_TX_INTERRUPT_ENABLE 0x06

#define R_RX_INTERRUPT_FLAGS 0x08

#define R_TX_INTERRUPT_FLAGS 0x0A

#define R_CONFIG_1 0x0C

#define R_CONFIG_2 0x0E

#define R_CONFIG_3 0x10

#define R_FMEA 0x13 - 1
#define R_MODEL 0x15 - 1
#define R_VERSION 0x17 - 1
#define R_RX_BYTE 0x19 - 1
#define R_RX_SPACE 0x1B - 1
#define R_TX_QUEUE_SELECTS 0x95 - 1
#define R_RX_READ_POINTER 0x97 - 1
#define R_RX_WRITE_POINTER 0x99 - 1
#define R_RX_NEXT_MESSAGE 0x9B - 1


// COMMANDS
#define CMD_WRITE_ALL 0x02
#define CMD_READ_ALL 0x03
#define CMD_CLR_RX_BUF 0xE0
#define CMD_CLR_TX_BUF 0x20

#define CMD_RD_MSG 0x91
#define CMD_RD_NXT_MSG 0x93

#define CMD_WR_NXT_LD_Q_L0 0xB0
#define CMD_WR_NXT_LD_Q_L1 0xB2
#define CMD_WR_NXT_LD_Q_L2 0xB4
#define CMD_WR_NXT_LD_Q_L3 0xB6
#define CMD_WR_NXT_LD_Q_L4 0xB8
#define CMD_WR_NXT_LD_Q_L5 0xBA
#define CMD_WR_NXT_LD_Q_L6 0xBC

#define CMD_WR_LD_Q_L0 0xC0
#define CMD_WR_LD_Q_L1 0xC2
#define CMD_WR_LD_Q_L2 0xC4
#define CMD_WR_LD_Q_L3 0xC6
#define CMD_WR_LD_Q_L4 0xC8
#define CMD_WR_LD_Q_L5 0xCA
#define CMD_WR_LD_Q_L6 0xCC

#define CMD_RD_LD_Q_L0 0xC1
#define CMD_RD_LD_Q_L1 0xC3
#define CMD_RD_LD_Q_L2 0xC5
#define CMD_RD_LD_Q_L3 0xC7
#define CMD_RD_LD_Q_L4 0xC9
#define CMD_RD_LD_Q_L5 0xCB
#define CMD_RD_LD_Q_L6 0xCD

#define CMD_HELLO_ALL 0x57

typedef enum
{
	SPI_GOOD,
	SPI_ERROR
} Spi_Status_E;

typedef enum
{
	BIT_CLEAR = 0,
	BIT_SET,
	MAX_BIT_STATUS
} Bit_Status_E;

typedef enum
{
	EQUALS = 0,
	NOT_EQUALS,
	AND,
	NOT_AND,
	OR,
	NOT_OR,
	NUM_OPERATORS
} OPERATOR_E;

void ssOn();
void ssOff();

void enableASCI();
void disableASCI();
void resetASCI();
uint8_t calcCrc(uint8_t* byteArr, uint32_t numBytes);

void sendSPI(uint8_t value);
void clearRxBuffer();
void clearTxBuffer();

bool clearRxIntFlags();
bool clearTxIntFlags();
bool writeRxIntStop(Bit_Status_E bitStatus);
bool writeRegisterBit(uint8_t registerAddress, uint8_t bitNumber, Bit_Status_E bitStatus);

uint8_t readRegister(uint8_t registerAddress);
void writeRegister(uint8_t registerAddress, uint8_t value);
bool rxErrorsExist();

bool readRxBusyFlag();
void clearRxBusyFlag();
bool readRxStopFlag();
void clearRxStopFlag();

bool writeAndVerifyRegister(uint8_t registerAddress, uint8_t value);
bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes);

bool readNextSpiMessage(uint8_t *data_p, uint32_t numBytesToRead);
bool writeAll(uint8_t address, uint16_t value, uint8_t *data_p, uint32_t numBmbs);
bool readAll(uint8_t address, uint32_t numBmbs, uint8_t *data_p);


#endif /* INC_SPIUTILS_H_ */
