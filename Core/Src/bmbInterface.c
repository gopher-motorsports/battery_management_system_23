/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "main.h"
#include "cmsis_os.h"
#include "bmbInterface.h"
#include "debug.h"
#include "bmbUtils.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define BYTES_PER_BMB_REGISTER 2
#define READ_CMD_LENGTH 	   1


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern osSemaphoreId asciSpiSemHandle;
extern osSemaphoreId asciSemHandle;
extern SPI_HandleTypeDef hspi1;

extern LeakyBucket_S asciCommsLeakyBucket;


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */


/* ==================================================================== */
/* ======================== GLOBAL VARIABLES ========================== */
/* ==================================================================== */



/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Enable ASCI SPI
*/
static void csOn();

/*!
  @brief   Disable ASCI SPI
*/
static void csOff();

/*!
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI
*/
static void sendAsciSpi(uint8_t value);

/*!
  @brief   Read a register on the ASCI
  @param   registerAddress - The register address to read from
  @return  The data contained in the register
*/
static uint8_t readRegister(uint8_t registerAddress);

/*!
  @brief   Write to a register on the ASCI
  @param   registerAddress - The register address to write to
  @param   value - The value to write to the register
*/
static void writeRegister(uint8_t registerAddress, uint8_t value);

/*!
  @brief   Write a value to a register and verify that the data was
  	  	   successfully written
  @param   registerAddress - The register address to write to
  @param   value - The byte to write to the register
  @return  True if the value was written and verified, false otherwise
*/
static bool writeAndVerifyRegister(uint8_t registerAddress, uint8_t value);

/*!
  @brief   Calculate the CRC for a given set of bytes
  @param   byteArr	Pointer to array for which to calculate CRC
  @param   numBytes	The number of bytes on which to calculate the CRC
  @return  uint8_t 	calculated CRC
*/
static uint8_t calcCrc(uint8_t* byteArr, uint32_t numBytes);

/*!
  @brief   Clears the RX buffer on the ASCI
*/
static void clearRxBuffer();

/*!
  @brief   Clears the TX buffer on the ASCI
*/
static void clearTxBuffer();

/*!
  @brief   Clears RX interrupt flags
  @return  True if success, false otherwise
*/
static bool clearRxIntFlags();

/*!
  @brief   Determine whether or not the RX busy flag has been set
  @return  True if set, false otherwise
*/
static bool readRxBusyFlag();

/*!
  @brief   Clear the RX busy flag
  @return  True if cleared, false otherwise
*/
static bool clearRxBusyFlag();

/*!
  @brief   Check whether or not RX error interupt flags were set
  @return  True if errors exist, false otherwise
*/
static bool rxErrorsExist();

/*!
  @brief   Load the TX queue on the ASCI and verify that the content was
  	  	   successfully written
  @param   data_p - Array containing data to be written to the queue
  @param   numBytes - Number of bytes to read from array to be written
  	  	   	   	      to the queue
  @return  True if success, false otherwise
*/
static bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes);

/*!
  @brief   Read the next SPI message in ASCI receive queue
  @param   data_p - Location where data should be written to
  @param   numBytesToRead - Number of bytes to read from queue to array
  @return  True if success, false otherwise
*/
static bool readNextSpiMessage(uint8_t **data_p, uint32_t numBytesToRead);

/*!
  @brief   Load a command into the ASCI from a buffer. Verify the contents of the load queue
		   and receive response from BMB Daisy Chain. Return results in a receive Buffer
  @param   sendBuffer - Pointer to the array containing data to be sent
  @param   recvBuffer - Pointer to the array where received data will be written to
  @param   numBytesToSend - Number of bytes to be sent from sendBuffer
  @param   numBytesToReceive - Number of bytes to be read into recvBuffer
  @return  True if successful transaction, false otherwise
*/
static bool sendReceiveMessageAsci(uint8_t* sendBuffer, uint8_t** recvBuffer, const uint32_t numBytesToSend, const uint32_t numBytesToReceive);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/*!
  @brief   Interrupt when ASCI INT Pin interrupt occurs
  @param   GPIO Pin causing interrupt
*/
//void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
//{
//	if (GPIO_Pin == GPIO_PIN_8)
//	{
//		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		xSemaphoreGiveFromISR(asciSpiSemHandle, &xHigherPriorityTaskWoken);
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}

/*!
  @brief   Enable ASCI SPI by pulling chip select low
*/
static void csOn()
{
	HAL_GPIO_WritePin(CS_ASCI_GPIO_Port, CS_ASCI_Pin, GPIO_PIN_RESET);
}

/*!
  @brief   Disable ASCI SPI by pulling chip select high
*/
static void csOff()
{
	HAL_GPIO_WritePin(CS_ASCI_GPIO_Port, CS_ASCI_Pin, GPIO_PIN_SET);
}

/*!
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI

*/
static void sendAsciSpi(uint8_t value)
{
	csOn();
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&value, 1);
	if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("Interrupt failed to occur during ASCI SPI transmit\n");
	}
	csOff();
}

/*!
  @brief   Read a register on the ASCI
  @param   registerAddress - The register address to read from
  @return  The data contained in the register
*/
static uint8_t readRegister(uint8_t registerAddress)
{
	csOn();
	// Since reading add 1 to address
	const uint8_t sendBuffer[2] = {registerAddress + 1};
	uint8_t recvBuffer[2] = {0};
	HAL_SPI_TransmitReceive_IT(&hspi1, (uint8_t *)&sendBuffer, (uint8_t *)&recvBuffer, 2);
	if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("Interrupt failed to occur during readRegister operation\n");
	}
	csOff();
	return recvBuffer[1];
}

/*!
  @brief   Write to a register on the ASCI
  @param   registerAddress - The register address to write to
  @param   value - The value to write to the register
*/
static void writeRegister(uint8_t registerAddress, uint8_t value)
{
	csOn();
	uint8_t sendBuffer[2] = {registerAddress, value};
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&sendBuffer, 2);
	if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("Interrupt failed to occur during writeRegister operation\n");
	}
	csOff();
}

/*!
  @brief   Write a value to a register and verify that the data was
  	  	   successfully written
  @param   registerAddress - The register address to write to
  @param   value - The byte to write to the register
  @return  True if the value was written and verified, false otherwise
*/
static bool writeAndVerifyRegister(uint8_t registerAddress, uint8_t value)
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(registerAddress, value);
		if (readRegister(registerAddress) == value)
		{
			// Data verified - exit
			return true;
		}
	}
	Debug("Failed to write and verify register\n");
	return false;
}

/*!
  @brief   Calculate the CRC for a given set of bytes
  @param   byteArr	Pointer to array for which to calculate CRC
  @param   numBytes	The number of bytes on which to calculate the CRC
  @return  uint8_t 	calculated CRC
*/
static uint8_t calcCrc(uint8_t* byteArr, uint32_t numBytes)
{
	uint8_t crc = 0x00;
	const uint8_t poly = 0xB2;
	for (int i = 0; i < numBytes; i++)
	{
		crc = crc ^ byteArr[i];
		for (int j = 0; j < 8; j++)
		{
			if (crc & 0x01)
			{
				crc = ((crc >> 1) ^ poly);
			}
			else
			{
				crc = (crc >> 1);
			}
		}
	}
	return crc;
}

/*!
  @brief   Clears the RX buffer on the ASCI
*/
static void clearRxBuffer()
{
	sendAsciSpi(CMD_CLR_RX_BUF);
}

/*!
  @brief   Clears the TX buffer on the ASCI
*/
static void clearTxBuffer()
{
	sendAsciSpi(CMD_CLR_TX_BUF);
}

/*!
  @brief   Clears RX interrupt flags
  @return  True if success, false otherwise
*/
static bool clearRxIntFlags()
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_FLAGS, 0x00);
		uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
		// TODO - double check why this is necessary?
		if ((result & ~(0x40)) == 0x00) { return true; }
	}
	Debug("Failed to clear Rx Interrupt Flags!\n");
	return false;
}

/*!
  @brief   Determine whether or not the RX busy flag has been set
  @return  True if set, false otherwise
*/
static bool readRxBusyFlag()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x20)
	{
		return true;
	}
	return false;
}

/*!
  @brief   Clear the RX busy flag
  @return  True if cleared, false otherwise
*/
static bool clearRxBusyFlag()
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x20));
		uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
		if ((result & (0x20)) == 0x00) { return true; }
	}
	Debug("Failed to clear Rx Busy Flag!\n");
	return false;
}

/*!
  @brief   Check whether or not RX error interupt flags were set
  @return  True if errors exist, false otherwise
*/
static bool rxErrorsExist()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x88)
	{
		Debug("Detected errors during transmission!\n");
		return true;
	}
	return false;
}

/*!
  @brief   Load the TX queue on the ASCI and verify that the content was
  	  	   successfully written
  @param   data_p - Array containing data to be written to the queue
  @param   numBytes - Number of bytes to read from array to be written
  	  	   	   	      to the queue
  @return  True if success, false otherwise
*/
static bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes)
{
	uint8_t sendBuffer[numBytes];
	memset(sendBuffer, 0, numBytes * sizeof(uint8_t));
	uint8_t recvBuffer[numBytes];
	memset(recvBuffer, 0, numBytes * sizeof(uint8_t));
	// Attempt to load the queue a set number of times before giving up
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool queueDataVerified = false;

		// Write queue
		csOn();
		HAL_SPI_Transmit_IT(&hspi1, data_p, numBytes);
		if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
		{
			Debug("Interrupt failed to occur while loading queue in loadAndVerifyTxQueue\n");
			csOff();
			continue;
		}
		csOff();

		// Read queue
		sendBuffer[0] = data_p[0] + 1;	// Read address is one greater than the write address
		csOn();
		HAL_SPI_TransmitReceive_IT(&hspi1, sendBuffer, recvBuffer, numBytes);
		if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
		{
			Debug("Interrupt failed to occur while reading queue contents in loadAndVerifyTxQueue\n");
			csOff();
			continue;
		}
		csOff();

		// Verify data in load queue - read back data should match data sent
		queueDataVerified = !(bool)memcmp(&data_p[1], &recvBuffer[1], numBytes - 1);
		if (queueDataVerified)
		{
			// Data integrity verified. Return true
			return true;
		}
	}
	Debug("Failed to load and verify TX queue\n");
	return false;
}

/*!
  @brief   Read the next SPI message in ASCI receive queue
  @param   data_p - Pointer to the array where data should
   be written to
  @param   numBytesToRead - Number of bytes to read from queue to array
  @return  True if success, false otherwise
*/
static bool readNextSpiMessage(uint8_t** data_p, uint32_t numBytesToRead)
{
	int arraySize = numBytesToRead + 1;	// Array needs to have space for command
	uint8_t sendBuffer[arraySize];
	memset(sendBuffer, 0, arraySize * sizeof(uint8_t));

	// Read numBytesToRead + 1 since we also need to send CMD_RD_NXT_MSG
	sendBuffer[0] = CMD_RD_NXT_MSG;
	csOn();
	HAL_SPI_TransmitReceive_IT(	&hspi1, sendBuffer, *data_p, arraySize);
	if (xSemaphoreTake(asciSpiSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("Interrupt failed to occur while reading next SPI message\n");
		csOff();
		return false;
	}
	csOff();
	// Return data should not include the CMD_RD_NXT_MSG
	(*data_p)++;
	return true;
}

/*!
  @brief   Load a command into the ASCI from a buffer. Verify the contents of the load queue
		   and receive response from BMB Daisy Chain. Return results in a receive Buffer
  @param   sendBuffer - Pointer to the array containing data to be sent
  @param   recvBuffer - Pointer to the array where received data will be written to
  @param   numBytesToSend - Number of bytes to be sent from sendBuffer
  @param   numBytesToReceive - Number of bytes to be read into recvBuffer
  @return  True if successful transaction, false otherwise
*/
static bool sendReceiveMessageAsci(uint8_t* sendBuffer, uint8_t** recvBuffer, const uint32_t numBytesToSend, const uint32_t numBytesToReceive)
{
	// TODO - see if there is a better way to do this
	clearRxBuffer();
	clearTxBuffer();
	// Send command to ASCI and verify data integrity
	if (!loadAndVerifyTxQueue(sendBuffer, numBytesToSend))
	{
		return false;
	}

	// Enable RX_Error, RX_Overflow and RX_Stop interrupts
	if(!writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0x8A))
	{
		return false;
	}

	if (!clearRxIntFlags())
	{
		return false;
	}

	sendAsciSpi(CMD_WR_NXT_LD_Q_L0);
	// Wait for ASCI interrupt to occur
	if (xSemaphoreTake(asciSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("ASCI Interrupt failed to occur during message transaction\n");
		return false;
	}

	// Verify that interrupt was caused by RX_Stop
	if ((readRegister(R_RX_STATUS) & 0x02) != 0x02)
	{
		return false;
	}

	// Read next SPI message.
	if (!readNextSpiMessage(recvBuffer, numBytesToReceive))
	{
		return false;
	}

	if (rxErrorsExist())
	{
		// TODO - do we want to have an error routine?
		return false;
	}
	
	return true;
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Power on ASCI
*/
void enableASCI()
{
	HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_SET);
}

/*!
  @brief   Power off ASCI
*/
void disableASCI()
{
	HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_RESET);
}

/*!
  @brief   Power cycle the ASCI
*/
void resetASCI()
{
	disableASCI();
	vTaskDelay(10);
	enableASCI();
}

/*!
  @brief   Initialize ASCI
  @return  True if successful initialization, false otherwise
*/
bool initASCI()
{
	resetASCI();
	csOff();
	bool successfulConfig = true;
	// dummy transaction since this chip sucks
	readRegister(R_CONFIG_3);

	// Set Keep_Alive to 0x05 = 160us
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_3, 0x05);

	// Enable RX_Error, RX_Overflow and RX_Busy interrupts
	successfulConfig &= writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0xA8);

	clearRxBuffer();

	// Enable TX_Preambles mode
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_2, 0x30);

	if (xSemaphoreTake(asciSemHandle, TIMEOUT_SPI_COMPLETE_MS) != pdTRUE)
	{
		Debug("Interrupt failed to occur while enabling TX_Preambles mode!\n");
		return false;
	}

	// Verify interrupt was caused by RX_Busy
	if (readRxBusyFlag())
	{
		clearRxBusyFlag();
	}
	else
	{
		return false;
	}

	// Verify RX_Busy_Status and RX_Empty_Status true
	successfulConfig &= (readRegister(R_RX_STATUS) == 0x21);

	// Enable RX_Stop INT
	successfulConfig &= writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0x8A);

	// Enable TX_Queue mode
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_2, 0x10);

	// Verify RX_Empty
	successfulConfig &= ((readRegister(R_RX_STATUS) & 0x01) == 0x01);

	clearTxBuffer();
	clearRxBuffer();

	return successfulConfig;
}

/*!
  @brief   Initialize BMB Daisy Chain. Enumerate BMBs
  @param   numBmbs - The number of BMBs detected in the daisy chain
  @return  True if successful initialization, false otherwise
*/
bool helloAll(uint32_t* numBmbs)
{
	const uint32_t bmbCmdLength = 0x03;					// CMD, REGISTER_ADDRESS, INITIALIZATION_ADDRESS
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = asciCmdLength + bmbCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));
	uint8_t recvBuffer[numBytesToReceive + READ_CMD_LENGTH];
	memset(recvBuffer, 0, (numBytesToReceive + READ_CMD_LENGTH) * sizeof(uint8_t));

	uint8_t* asciCmdBuffer = sendBuffer;
	uint8_t* bmbCmdBuffer  = &sendBuffer[asciCmdLength];

	// Send Hello_All command
	// ASCI CMD Data
	asciCmdBuffer[0] = CMD_WR_LD_Q_L0;
	asciCmdBuffer[1] = 0x03;			// Data length

	// BMB CMD Data
	bmbCmdBuffer[0] = CMD_HELLO_ALL;	// HELLOALL command byte
	bmbCmdBuffer[1] = 0x00;				// Register address
	bmbCmdBuffer[2] = 0x00;				// Initialization address for HELLOALL

	uint8_t* pRecvBuffer = recvBuffer;
	if (!sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive))
	{
		Debug("Error in HelloAll!\n");
		updateLeakyBucketFail(&asciCommsLeakyBucket);
		return false;
	}
	
	// Number of BMBs is last byte in the received message
	*numBmbs = pRecvBuffer[bmbCmdLength - 1];
	updateLeakyBucketSuccess(&asciCommsLeakyBucket);
	return true;
}

/*!
  @brief   Write data to all registers on BMBs
  @param   address - BMB register address to write to
  @param   value - Value to write to BMB register
  @param   numBmbs - The number of BMBs we expect to write to
  @return  True if success, false otherwise
*/
bool writeAll(uint8_t address, uint16_t value, uint32_t numBmbs)
{
	const uint32_t bmbCmdLength = 0x06;					// CMD, address, LSB, MSB, CRC, ALIVE_COUNTER
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = bmbCmdLength + asciCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));
	// Are these large enough? Because we need the command byte as well
	uint8_t recvBuffer[numBytesToReceive + READ_CMD_LENGTH];
	memset(recvBuffer, 0, (numBytesToReceive + READ_CMD_LENGTH) * sizeof(uint8_t));

	uint8_t* asciCmdBuffer = sendBuffer;
	uint8_t* bmbCmdBuffer  = &sendBuffer[asciCmdLength];

	// ASCI Command Data
	asciCmdBuffer[0] = CMD_WR_LD_Q_L0;				// Command for ASCI
	asciCmdBuffer[1] = numBytesToReceive;			// Data length for ASCI

	// BMB Command Data
	bmbCmdBuffer[0] = CMD_WRITE_ALL;				// Command byte for BMBs
	bmbCmdBuffer[1] = address;						// Register Address for BMBs
	bmbCmdBuffer[2] = (uint8_t)(value & 0x00FF);	// LSB
	bmbCmdBuffer[3] = (uint8_t)(value >> 8);		// MSB
	bmbCmdBuffer[4] = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, LSB, MSB
	bmbCmdBuffer[5] = 0x00;							// Alive counter seed value for BMBs

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool writeAllSuccess = true;

		// ASCI message transaction
		uint8_t* pRecvBuffer = recvBuffer;
		writeAllSuccess &= sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive);

		// Verify BMB Command Data. Do not check last byte (alive-counter) as this will be different
		writeAllSuccess &= !(bool)memcmp(bmbCmdBuffer, pRecvBuffer, bmbCmdLength - 1);

		// Verify the alive counter
		writeAllSuccess &= (pRecvBuffer[bmbCmdLength - 1] == numBmbs);

		if (writeAllSuccess)
		{
			updateLeakyBucketSuccess(&asciCommsLeakyBucket);
			return true;
		}
	}
	Debug("Failed to write all\n");
	updateLeakyBucketFail(&asciCommsLeakyBucket);
	return false;
}

/*!
  @brief   Write data to register on single BMB
  @param   address - BMB register address to write to
  @param   value - Value to write to BMB register
  @param   bmbIndex - The index of the target BMB to write to
  @return  True if success, false otherwise
*/
bool writeDevice(uint8_t address, uint16_t value, uint32_t bmbIndex)
{
	const uint32_t bmbCmdLength = 0x06;					// CMD, address, LSB, MSB, CRC, ALIVE_COUNTER
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = bmbCmdLength + asciCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength;
	const uint8_t  bmbAddress = (bmbIndex << 3) | 0b100;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));
	uint8_t recvBuffer[numBytesToReceive + READ_CMD_LENGTH];
	memset(recvBuffer, 0, (numBytesToReceive + READ_CMD_LENGTH) * sizeof(uint8_t));

	uint8_t* asciCmdBuffer = sendBuffer;
	uint8_t* bmbCmdBuffer  = &sendBuffer[asciCmdLength];

	// ASCI Command Data
	asciCmdBuffer[0] = CMD_WR_LD_Q_L0;				// Command for ASCI
	asciCmdBuffer[1] = numBytesToReceive;			// Data length for ASCI

	// BMB Command Data
	bmbCmdBuffer[0] = bmbAddress;					// Address to select individual BMB
	bmbCmdBuffer[1] = address;						// Register Address for BMBs
	bmbCmdBuffer[2] = (uint8_t)(value & 0x00FF);	// LSB
	bmbCmdBuffer[3] = (uint8_t)(value >> 8);		// MSB
	bmbCmdBuffer[4] = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, LSB, MSB
	bmbCmdBuffer[5] = 0x00;							// Alive counter seed value for BMBs

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool writeAllSuccess = true;

		// ASCI message transaction
		uint8_t* pRecvBuffer = recvBuffer;
		writeAllSuccess &= sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive);

		// Verify BMB Command Data. Do not check last byte (alive-counter) as this will be different
		writeAllSuccess &= !(bool)memcmp(bmbCmdBuffer, pRecvBuffer, bmbCmdLength - 1);

		// Verify the alive counter
		writeAllSuccess &= (pRecvBuffer[bmbCmdLength - 1] == 1);

		if (writeAllSuccess)
		{
			updateLeakyBucketSuccess(&asciCommsLeakyBucket);
			return true;
		}
	}
	Debug("Failed to write device\n");
	updateLeakyBucketFail(&asciCommsLeakyBucket);
	return false;
}

/*!
  @brief   Read data from a register on all BMBs
  @param   address - BMB register address to read from
  @param   data_p - Array to read in the data to
  @param   numBmbs - The number of BMBs we expect to read from
  @return  True if success, false otherwise
*/
bool readAll(uint8_t address, uint8_t *data_p, uint32_t numBmbs)
{
	const uint32_t bmbCmdLength = 0x05;					// CMD, address, DATA_CHECK, CRC, ALIVE_COUNTER
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = bmbCmdLength + asciCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength + numBmbs * BYTES_PER_BMB_REGISTER;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));

	uint8_t* asciCmdBuffer = sendBuffer;
	uint8_t* bmbCmdBuffer  = &sendBuffer[asciCmdLength];

	// ASCI Command Data
	asciCmdBuffer[0] = CMD_WR_LD_Q_L0;				// Command for ASCI
	asciCmdBuffer[1] = numBytesToReceive;			// Data length for ASCI

	// BMB Command Data
	bmbCmdBuffer[0] = CMD_READ_ALL;					// Command byte for BMBs
	bmbCmdBuffer[1] = address;						// Register Address for BMBs
	bmbCmdBuffer[2] = 0x00;							// Data check byte
	bmbCmdBuffer[3] = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, DATA_CHECK
	bmbCmdBuffer[4] = 0x00;							// Alive counter seed value for BMBs

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readAllSuccess = true;

		uint8_t* pRecvBuffer = data_p;
		readAllSuccess &= sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive);

		// Calculate CRC code based on received data
		const uint8_t calculatedCrc = calcCrc(pRecvBuffer, numBytesToReceive - 2); // Do not read PEC byte and alive counter byte
		uint8_t recvCrc = pRecvBuffer[3 + (BYTES_PER_BMB_REGISTER * numBmbs)];

		// Verify data CRC
		readAllSuccess &= (calculatedCrc == recvCrc);
		// Verify Alive-counter byte
		readAllSuccess &= (pRecvBuffer[4 + (BYTES_PER_BMB_REGISTER * numBmbs)] == numBmbs);

		if (readAllSuccess)
		{
			updateLeakyBucketSuccess(&asciCommsLeakyBucket);
			return true;
		}
	}
	Debug("Failed to read all\n");
	updateLeakyBucketFail(&asciCommsLeakyBucket);
	return false;
}

/*!
  @brief   Read data from register on single BMB
  @param   address - BMB register address to read from
  @param   data_p - Array to read in the data to
  @param   bmbIndex - The index of the target BMB to read from
  @return  True if success, false otherwise
*/
bool readDevice(uint8_t address, uint8_t *data_p, uint32_t bmbIndex)
{
	const uint32_t bmbCmdLength = 0x05;					// CMD, address, DATA_CHECK, CRC, ALIVE_COUNTER
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = bmbCmdLength + asciCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength + BYTES_PER_BMB_REGISTER;
	const uint8_t  bmbAddress = (bmbIndex << 3) | 0b101;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));

	uint8_t* asciCmdBuffer = sendBuffer;
	uint8_t* bmbCmdBuffer  = &sendBuffer[asciCmdLength];

	// ASCI Command Data
	asciCmdBuffer[0] = CMD_WR_LD_Q_L0;				// Command for ASCI
	asciCmdBuffer[1] = numBytesToReceive;			// Data length for ASCI

	// BMB Command Data
	bmbCmdBuffer[0] = bmbAddress;					// Command byte for BMBs
	bmbCmdBuffer[1] = address;						// Register Address for BMBs
	bmbCmdBuffer[2] = 0x00;							// Data check byte
	bmbCmdBuffer[3] = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, DATA_CHECK
	bmbCmdBuffer[4] = 0x00;							// Alive counter seed value for BMBs

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readAllSuccess = true;

		uint8_t* pRecvBuffer = data_p;
		readAllSuccess &= sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive);

		// Calculate CRC code based on received data
		const uint8_t calculatedCrc = calcCrc(pRecvBuffer, numBytesToReceive - 2); // Do not read PEC byte and alive counter byte
		uint8_t recvCrc = pRecvBuffer[numBytesToReceive - 2];

		// Verify data CRC
		readAllSuccess &= (calculatedCrc == recvCrc);
		// Verify Alive-counter byte
		readAllSuccess &= (pRecvBuffer[numBytesToReceive - 1] == 1);

		if (readAllSuccess)
		{
			updateLeakyBucketSuccess(&asciCommsLeakyBucket);
			return true;
		}
	}
	Debug("Failed to read device\n");
	updateLeakyBucketFail(&asciCommsLeakyBucket);
	return false;
}
