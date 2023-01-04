#ifndef INC_SPIUTILS_H_
#define INC_SPIUTILS_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define SPI_BUFF_SIZE 64
// The number of read + write attempts we will do to verify the integrity of the data written
#define NUM_DATA_CHECKS 3

// REGISTERS
#define R_RX_STATUS 0x01 - 1			// Non writable
#define R_TX_STATUS 0x03 - 1			// Non writable
#define R_RX_INTERRUPT_ENABLE 0x04
#define R_TX_INTERRUPT_ENABLE 0x06
#define R_RX_INTERRUPT_FLAGS 0x08
#define R_TX_INTERRUPT_FLAGS 0x0A
#define R_CONFIG_1 0x0C
#define R_CONFIG_2 0x0E
#define R_CONFIG_3 0x10
#define R_FMEA 0x13 - 1					// Non writable
#define R_MODEL 0x15 - 1				// Non writable
#define R_VERSION 0x17 - 1				// Non writable
#define R_RX_BYTE 0x19 - 1				// Non writable
#define R_RX_SPACE 0x1B - 1				// Non writable
#define R_TX_QUEUE_SELECTS 0x95 - 1		// Non writable
#define R_RX_READ_POINTER 0x97 - 1		// Non writable
#define R_RX_WRITE_POINTER 0x99 - 1		// Non writable
#define R_RX_NEXT_MESSAGE 0x9B - 1		// Non writable

// COMMANDS
#define CMD_WRITE_ALL 0x02          // Write to all BMB registers
#define CMD_READ_ALL 0x03           // Read from all BMB registers
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


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */
/*!
  @brief   Enable ASCI SPI
*/
void csAsciOn();

/*!
  @brief   Disable ASCI SPI
*/
void csAsciOff();

/*!
  @brief   Power on ASCI
*/
void enableASCI();

/*!
  @brief   Power off ASCI
*/
void disableASCI();

/*!
  @brief   Power cycle the ASCI
*/
void resetASCI();

/*!
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI
*/
void sendAsciSpi(uint8_t value);

/*!
  @brief   Read a register on the ASCI
  @param   registerAddress - The register address to read from
  @return  The data contained in the register
*/
uint8_t readRegister(uint8_t registerAddress);

/*!
  @brief   Write to a register on the ASCI
  @param   registerAddress - The register address to write to
  @param   value - The value to write to the register
*/
void writeRegister(uint8_t registerAddress, uint8_t value);

/*!
  @brief   Write a value to a register and verify that the data was
  	  	   successfully written
  @param   registerAddress - The register address to write to
  @param   value - The byte to write to the register
  @return  True if the value was written and verified, false otherwise
*/
bool writeAndVerifyRegister(uint8_t registerAddress, uint8_t value);


/*!
  @brief   Calculate the CRC for a given set of bytes
  @param   byteArr	Pointer to array for which to calculate CRC
  @param   numBytes	The number of bytes on which to calculate the CRC
  @return  uint8_t 	calculated CRC
*/
uint8_t calcCrc(uint8_t* byteArr, uint32_t numBytes);

/*!
  @brief   Clears the RX buffer on the ASCI
*/
void clearRxBuffer();

/*!
  @brief   Clears the TX buffer on the ASCI
*/
void clearTxBuffer();

/*!
  @brief   Clears RX interrupt flags
  @return  True if success, false otherwise
*/
bool clearRxIntFlags();

/*!
  @brief   Clears TX interrupt flags
  @return  True if success, false otherwise
*/
bool clearTxIntFlags();

/*!
  @brief   Determine whether or not the RX busy flag has been set
  @return  True if set, false otherwise
*/
bool readRxBusyFlag();

/*!
  @brief   Clear the RX busy flag
  @return  True if cleared, false otherwise
*/
bool clearRxBusyFlag();

/*!
  @brief   Check the status of the RX stop flag
  @return  True if flag set, false otherwise
*/
bool readRxStopFlag();

/*!
  @brief   Clear the RX stop flag
*/
bool clearRxStopFlag();

/*!
  @brief   Check whether or not RX error interupt flags were set
  @return  True if errors exist, false otherwise
*/
bool rxErrorsExist();

/*!
  @brief   Enable RX Stop Interrupt on ASCI
  @return  True if success, false otherwise
*/
bool writeRxIntStop(bool bitSet);

/*!
  @brief   Load the TX queue on the ASCI and verify that the content was
  	  	   successfully written
  @param   data_p - Array containing data to be written to the queue
  @param   numBytes - Number of bytes to read from array to be written
  	  	   	   	      to the queue
  @return  True if success, false otherwise
*/
bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes);

/*!
  @brief   Read the next SPI message in ASCI receive queue
  @param   data_p - Location where data should be written to
  @param   numBytesToRead - Number of bytes to read from queue to array
  @return  True if success, false otherwise
*/
bool readNextSpiMessage(uint8_t **data_p, uint32_t numBytesToRead);

/*!
  @brief   Write data to all registers on BMBs
  @param   address - BMB register address to write to
  @param   value - Value to write to BMB register
  @param   numBmbs - The number of BMBs we expect to write to
  @return  True if success, false otherwise
*/
bool writeAll(uint8_t address, uint16_t value, uint32_t numBmbs);

/*!
  @brief   Write data to a register on single a BMB
  @param   address - BMB register address to write to
  @param   value - Value to write to BMB register
  @param   bmbIndex - The index of the target BMB to write to
  @return  True if success, false otherwise
*/
bool writeDevice(uint8_t address, uint16_t value, uint32_t bmbIndex);

/*!
  @brief   Read data from a register on all BMBs
  @param   address - BMB register address to read from
  @param   data_p - Array to read in the data to
  @param   numBmbs - The number of BMBs we expect to read from
  @return  True if success, false otherwise
*/
bool readAll(uint8_t address, uint8_t *data_p, uint32_t numBmbs);

/*!
  @brief   Read data from a register on a single BMB
  @param   address - BMB register address to read from
  @param   data_p - Array to read in the data to
  @param   bmbIndex - The index of the target BMB to read from
  @return  True if success, false otherwise
*/
bool readDevice(uint8_t address, uint8_t *data_p, uint32_t bmbIndex);


#endif /* INC_SPIUTILS_H_ */
