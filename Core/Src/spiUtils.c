/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "main.h"
#include "cmsis_os.h"
#include "spiUtils.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define BYTES_PER_BMB_REGISTER 2


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
extern osSemaphoreId binSemHandle;
extern SPI_HandleTypeDef hspi1;


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
void csAsciOn();

/*!
  @brief   Disable ASCI SPI
*/
void csAsciOff();

/*!
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI
*/
void sendAsciSpi(uint8_t value);


bool sendReceiveMessageAsci(uint8_t* sendBuffer, uint8_t** recvBuffer, const uint32_t numBytesToSend, const uint32_t numBytesToReceive);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */
/*!
  @brief   Interrupt when SPI TX finishes. Unblock task by releasing
  	  	   semaphore
  @param   SPI Handle
*/
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi1)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*!
  @brief   Interrupt when SPI TX/RX finishes. Unblock task by releasing
  	  	   semaphore
  @param   SPI Handle
*/
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi1)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

/*!
  @brief   Interrupt when ASCI INT Pin interrupt occurs
  @param   GPIO Pin causing interrupt
*/
//void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
//{
//	if (GPIO_Pin == GPIO_PIN_8)
//	{
//		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
//		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	}
//}

bool sendReceiveMessageAsci(uint8_t* sendBuffer, uint8_t** recvBuffer, const uint32_t numBytesToSend, const uint32_t numBytesToReceive)
{
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
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("ASCI Interrupt failed to occur during WRITEALL\n");
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
  @brief   Enable ASCI SPI by pulling chip select low
*/
void csAsciOn()
{
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
}

/*!
  @brief   Disable ASCI SPI by pulling chip select high
*/
void csAsciOff()
{
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_SET);
}

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
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI

*/
void sendAsciSpi(uint8_t value)
{
	csAsciOn();
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&value, 1);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during SPI transmit\n");
	}
	csAsciOff();
}

/*!
  @brief   Read a register on the ASCI
  @param   registerAddress - The register address to read from
  @return  The data contained in the register
*/
uint8_t readRegister(uint8_t registerAddress)
{
	csAsciOn();
	// Since reading add 1 to address
	const uint8_t sendBuffer[2] = {registerAddress + 1};
	uint8_t recvBuffer[2] = {0};
	HAL_SPI_TransmitReceive_IT(&hspi1, (uint8_t *)&sendBuffer, (uint8_t *)&recvBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during readRegister operation\n");
	}
	csAsciOff();
	return recvBuffer[1];
}

/*!
  @brief   Write to a register on the ASCI
  @param   registerAddress - The register address to write to
  @param   value - The value to write to the register
*/
void writeRegister(uint8_t registerAddress, uint8_t value)
{
	csAsciOn();
	uint8_t sendBuffer[2] = {registerAddress, value};
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&sendBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during writeRegister operation\n");
	}
	csAsciOff();
}

/*!
  @brief   Write a value to a register and verify that the data was
  	  	   successfully written
  @param   registerAddress - The register address to write to
  @param   value - The byte to write to the register
  @return  True if the value was written and verified, false otherwise
*/
bool writeAndVerifyRegister(uint8_t registerAddress, uint8_t value)
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
	printf("Failed to write and verify register\n");
	return false;
}

/*!
  @brief   Calculate the CRC for a given set of bytes
  @param   byteArr	Pointer to array for which to calculate CRC
  @param   numBytes	The number of bytes on which to calculate the CRC
  @return  uint8_t 	calculated CRC
*/
uint8_t calcCrc(uint8_t* byteArr, uint32_t numBytes)
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
void clearRxBuffer()
{
	sendAsciSpi(CMD_CLR_RX_BUF);
}

/*!
  @brief   Clears the TX buffer on the ASCI
*/
void clearTxBuffer()
{
	sendAsciSpi(CMD_CLR_TX_BUF);
}

/*!
  @brief   Clears RX interrupt flags
  @return  True if success, false otherwise
*/
bool clearRxIntFlags()
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_FLAGS, 0x00);
		uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
		// TODO - double check why this is necessary?
		if ((result & ~(0x40)) == 0x00) { return true; }
	}
	printf("Failed to clear Rx Interrupt Flags!\n");
	return false;
}

/*!
  @brief   Clears TX interrupt flags
  @return  True if success, false otherwise
*/
bool clearTxIntFlags()
{
	return writeAndVerifyRegister(R_TX_INTERRUPT_FLAGS, 0x00);
}

/*!
  @brief   Determine whether or not the RX busy flag has been set
  @return  True if set, false otherwise
*/
bool readRxBusyFlag()
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
bool clearRxBusyFlag()
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x20));
		uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
		if ((result & (0x20)) == 0x00) { return true; }
	}
	printf("Failed to clear Rx Busy Flag!\n");
	return false;
}

/*!
  @brief   Check the status of the RX stop flag
  @return  True if flag set, false otherwise
*/
bool readRxStopFlag()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x02)
	{
		return true;
	}
	return false;
}

/*!
  @brief   Clear the RX stop flag
  @return  True if cleared, false otherwise
*/
bool clearRxStopFlag()
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x02));
		uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
		if ((result & (0x02)) == 0x00) { return true; }
	}
	printf("Failed to clear the Rx Stop Flag!\n");
	return false;
}

/*!
  @brief   Check whether or not RX error interupt flags were set
  @return  True if errors exist, false otherwise
*/
bool rxErrorsExist()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x88)
	{
		printf("Detected errors during transmission!\n");
		return true;
	}
	return false;
}

/*!
  @brief   Enable RX Stop Interrupt on ASCI
  @return  True if success, false otherwise
*/
bool writeRxIntStop(bool bitSet)
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		writeRegister(R_RX_INTERRUPT_ENABLE, (0x02));
		uint8_t result = readRegister(R_RX_INTERRUPT_ENABLE);
		if ((result & (0x02)) == 0x02) { return true; }
	}
	printf("Failed to write RxIntStop!\n");
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
bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes)
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
		csAsciOn();
		HAL_SPI_Transmit_IT(&hspi1, data_p, numBytes);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur while loading queue in loadAndVerifyTxQueue\n");
			csAsciOff();
			continue;
		}
		csAsciOff();

		// Read queue
		sendBuffer[0] = data_p[0] + 1;	// Read address is one greater than the write address
		csAsciOn();
		HAL_SPI_TransmitReceive_IT(&hspi1, sendBuffer, recvBuffer, numBytes);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur while reading queue contents in loadAndVerifyTxQueue\n");
			csAsciOff();
			continue;
		}
		csAsciOff();

		// Verify data in load queue - read back data should match data sent
		queueDataVerified = !(bool)memcmp(&data_p[1], &recvBuffer[1], numBytes - 1);
		if (queueDataVerified)
		{
			// Data integrity verified. Return true
			return true;
		}
	}
	printf("Failed to load and verify TX queue\n");
	return false;
}

/*!
  @brief   Read the next SPI message in ASCI receive queue
  @param   data_p - Location where data should be written to
  @param   numBytesToRead - Number of bytes to read from queue to array
  @return  True if success, false otherwise
*/
bool readNextSpiMessage(uint8_t** data_p, uint32_t numBytesToRead)
{
	int arraySize = numBytesToRead + 1;	// Array needs to have space for command
	uint8_t sendBuffer[arraySize];
	memset(sendBuffer, 0, arraySize * sizeof(uint8_t));

	// Read numBytesToRead + 1 since we also need to send CMD_RD_NXT_MSG
	sendBuffer[0] = CMD_RD_NXT_MSG;
	csAsciOn();
	HAL_SPI_TransmitReceive_IT(	&hspi1, sendBuffer, *data_p, arraySize);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur while reading next SPI message\n");
		csAsciOff();
		return false;
	}
	csAsciOff();
	// Return data should not include the CMD_RD_NXT_MSG
	(*data_p)++;
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
	uint8_t recvBuffer[numBytesToReceive];
	memset(recvBuffer, 0, numBytesToReceive * sizeof(uint8_t));

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
	const uint8_t crc = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, LSB, MSB
	bmbCmdBuffer[4] = crc;							// PEC byte
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
			return true;
		}
	}
	printf("Failed to write all\n");
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
	uint8_t recvBuffer[numBytesToReceive];
	memset(recvBuffer, 0, numBytesToReceive * sizeof(uint8_t));

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
	const uint8_t crc = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, LSB, MSB
	bmbCmdBuffer[4] = crc;							// PEC byte
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
			return true;
		}
	}
	printf("Failed to write Device\n");
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
	const uint8_t crc = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, DATA_CHECK
	bmbCmdBuffer[3] = crc;							// PEC byte
	bmbCmdBuffer[4] = 0x00;							// Alive counter seed value for BMBs

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readAllSuccess = true;

		uint8_t* pRecvBuffer = data_p;
		readAllSuccess &= sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToReceive);

		// Calculate CRC code based on received data
		const uint8_t calculatedCrc = calcCrc(pRecvBuffer, numBytesToReceive - 2); // Do not read PEC byte and alive counter byte
		uint8_t recvCrc = pRecvBuffer[3 + (2 * numBmbs)];

		// Verify data CRC
		readAllSuccess &= (calculatedCrc == recvCrc);
		// Verify Alive-counter byte
		readAllSuccess &= (pRecvBuffer[4 + (2 * numBmbs)] == numBmbs);

		if (readAllSuccess)
		{
			return true;
		}
	}
	printf("Failed to READALL\n");
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
	const uint8_t crc = calcCrc(bmbCmdBuffer, bmbCmdLength - 2);	// Calculate CRC on CMD, ADDRESS, DATA_CHECK
	bmbCmdBuffer[3] = crc;							// PEC byte
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
			return true;
		}
	}
	printf("Failed to READALL\n");
	return false;
}

/*!
  @brief   Initialize ASCI and BMB daisy chain. Enumerate BMBs
  @param   numBmbs - Updated with number of enumerated BMBs from HELLOALL command
  @return  True if successful initialization, false otherwise
*/
bool initASCI()
{
	resetASCI();
	csAsciOff();
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

	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur while enabling TX_Preambles mode!\n");
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

	// Enable TX_Queue mode
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_2, 0x10);

	// Verify RX_Empty
	successfulConfig &= ((readRegister(R_RX_STATUS) & 0x01) == 0x01);

	clearTxBuffer();
	clearRxBuffer();

	return successfulConfig;
}

bool helloAll(uint32_t* numBmbs)
{
	const uint32_t bmbCmdLength = 0x03;					// CMD, REGISTER_ADDRESS, INITIALIZATION_ADDRESS
	const uint32_t asciCmdLength = 0x02;				// CMD, DATA_LENGTH
	const uint32_t numBytesToSend = asciCmdLength + bmbCmdLength;
	const uint32_t numBytesToReceive = bmbCmdLength;

	uint8_t sendBuffer[numBytesToSend];
	memset(sendBuffer, 0, numBytesToSend * sizeof(uint8_t));
	uint8_t recvBuffer[numBytesToReceive];
	memset(recvBuffer, 0, numBytesToReceive * sizeof(uint8_t));

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
	if (!sendReceiveMessageAsci(sendBuffer, &pRecvBuffer, numBytesToSend, numBytesToSend))
	{
		return false;
	}
	
	*numBmbs = pRecvBuffer[2];
	return true;
}
