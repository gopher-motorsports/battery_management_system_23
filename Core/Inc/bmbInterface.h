#ifndef INC_BMBINTERFACE_H_
#define INC_BMBINTERFACE_H_


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

// Timeout
#define TIMEOUT_SPI_COMPLETE_MS 10

// REGISTERS
#define R_RX_STATUS             (0x01 - 1)		// Non writable
#define R_TX_STATUS             (0x03 - 1)		// Non writable
#define R_RX_INTERRUPT_ENABLE   0x04
#define R_TX_INTERRUPT_ENABLE   0x06
#define R_RX_INTERRUPT_FLAGS    0x08
#define R_TX_INTERRUPT_FLAGS    0x0A
#define R_CONFIG_1              0x0C
#define R_CONFIG_2              0x0E
#define R_CONFIG_3              0x10
#define R_FMEA                  (0x13 - 1)		// Non writable
#define R_MODEL                 (0x15 - 1)		// Non writable
#define R_VERSION               (0x17 - 1)		// Non writable
#define R_RX_BYTE               (0x19 - 1)		// Non writable
#define R_RX_SPACE              (0x1B - 1)		// Non writable
#define R_TX_QUEUE_SELECTS      (0x95 - 1)		// Non writable
#define R_RX_READ_POINTER       (0x97 - 1)		// Non writable
#define R_RX_WRITE_POINTER      (0x99 - 1)		// Non writable
#define R_RX_NEXT_MESSAGE       (0x9B - 1)		// Non writable

// COMMANDS
#define CMD_WRITE_ALL 		  0x02        // Write to all BMB registers
#define CMD_READ_ALL 		    0x03        // Read from all BMB registers
#define CMD_CLR_TX_BUF 		  0x20        // Clear Transmit Buffer
#define CMD_RD_MSG 			    0x91        // Read Message
#define CMD_CLR_RX_BUF 		  0xE0        // Clear Receive Buffer
#define CMD_RD_NXT_MSG 		  0x93        // Read Next Message
#define CMD_WR_NXT_LD_Q_L0	0xB0        // Write Next Load Queue
#define CMD_WR_NXT_LD_Q_L1	0xB2
#define CMD_WR_NXT_LD_Q_L2	0xB4
#define CMD_WR_NXT_LD_Q_L3	0xB6
#define CMD_WR_NXT_LD_Q_L4	0xB8
#define CMD_WR_NXT_LD_Q_L5	0xBA
#define CMD_WR_NXT_LD_Q_L6	0xBC
#define CMD_WR_LD_Q_L0		  0xC0	      // Write Load Queue
#define CMD_WR_LD_Q_L1		  0xC2
#define CMD_WR_LD_Q_L2		  0xC4
#define CMD_WR_LD_Q_L3		  0xC6
#define CMD_WR_LD_Q_L4		  0xC8
#define CMD_WR_LD_Q_L5		  0xCA
#define CMD_WR_LD_Q_L6		  0xCC
#define CMD_RD_LD_Q_L0		  0xC1
#define CMD_RD_LD_Q_L1		  0xC3
#define CMD_RD_LD_Q_L2		  0xC5
#define CMD_RD_LD_Q_L3		  0xC7
#define CMD_RD_LD_Q_L4		  0xC9
#define CMD_RD_LD_Q_L5		  0xCB
#define CMD_RD_LD_Q_L6		  0xCD
#define CMD_HELLO_ALL 		  0x57	      // Hello All Initialization


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

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
  @brief   Clears the RX buffer on the ASCI
*/
void clearRxBuffer();

/*!
  @brief   Clears the TX buffer on the ASCI
*/
void clearTxBuffer();

/*!
  @brief   Initialize ASCI and BMB daisy chain. Enumerate BMBs
  @param   numBmbs - Updated with number of enumerated BMBs from HELLOALL command
  @return  True if successful initialization, false otherwise
*/
bool initASCI();

/*!
  @brief   Initialize BMB Daisy Chain. Enumerate BMBs
  @param   numBmbs - The number of BMBs detected in the daisy chain
  @return  True if successful initialization, false otherwise
*/
bool helloAll(uint32_t* numBmbs);

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

#endif /* INC_BMBINTERFACE_H_ */
