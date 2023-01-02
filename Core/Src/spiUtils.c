/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "main.h"
#include "cmsis_os.h"
#include "spiUtils.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */



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


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */
/*!
  @brief   Enable ASCI SPI
*/
void ssOn()
{
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
}

/*!
  @brief   Disable ASCI SPI
*/
void ssOff()
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
	vTaskDelay(10 / portTICK_PERIOD_MS);
	enableASCI();
}

/*!
  @brief   Send a byte on SPI
  @param   value - byte to send over SPI
*/
void sendSPI(uint8_t value)
{
	ssOn();
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&value, 1);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during SPI transmit\n");
	}
	ssOff();
}

/*!
  @brief   Read a register on the ASCI
  @param   registerAddress - The register address to read from
  @return  The data contained in the register
*/
uint8_t readRegister(uint8_t registerAddress)
{
	ssOn();
	// Since reading add 1 to address
	const uint8_t sendBuffer[2] = {registerAddress + 1};
	uint8_t recvBuffer[2] = {0};
	HAL_SPI_TransmitReceive_IT(&hspi1, (uint8_t *)&sendBuffer, (uint8_t *)&recvBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during readRegister operation\n");
	}
	ssOff();
	return recvBuffer[1];
}

/*!
  @brief   Write to a register on the ASCI
  @param   registerAddress - The register address to write to
  @param   value - The value to write to the register
*/
void writeRegister(uint8_t registerAddress, uint8_t value)
{
	ssOn();
	uint8_t sendBuffer[2] = {registerAddress, value};
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&sendBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during writeRegister operation\n");
	}
	ssOff();
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
	sendSPI(CMD_CLR_RX_BUF);
}

/*!
  @brief   Clears the TX buffer on the ASCI
*/
void clearTxBuffer()
{
	sendSPI(CMD_CLR_TX_BUF);
}

/*!
  @brief   Clears RX interrupt flags
  @return  True if success, false otherwise
*/
bool clearRxIntFlags()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, 0x00);
	uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
	return ((result & ~(0x40)) == 0x00);
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
*/
void clearRxBusyFlag()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x20));
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
*/
void clearRxStopFlag()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x02));
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
	return writeRegisterBit(R_RX_INTERRUPT_ENABLE, 1U, bitSet);
}

// TODO: Add multiple attempts
/*!
  @brief   Write to a specific bit in a register
  @param   registerAddress - The address of the register to be modified
  @param   bitNumber - 0-indexed bit to be modified starting with LSB
  @param   bitSet - If true, bit set to 1, if false, bit set to 0
  @return  True if success, false otherwise
*/
bool writeRegisterBit(uint8_t registerAddress, uint8_t bitNumber, bool bitSet)
{
	if (bitNumber >= 8) return false;	// Invalid input

	const uint8_t bitMask = (0x01 << bitNumber);
	const uint8_t regData = readRegister(registerAddress);

	if (bitSet)
	{
		writeRegister(registerAddress, (regData | bitMask));
	}
	else
	{
		writeRegister(registerAddress, (regData & (~bitMask)));
	}
	// TODO: Check this line
	if (!!(readRegister(registerAddress) & bitMask) == bitSet)
	{
		return true;
	}
	else
	{
		return false;
	}
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
	uint8_t recvBuffer[numBytes];
	uint8_t sendBuffer[numBytes];
	memset(recvBuffer, 0, numBytes * sizeof(uint8_t));
	memset(sendBuffer, 0, numBytes * sizeof(uint8_t));
	// Attempt to load the queue a set number of times before giving up
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool queueDataVerified = false;

		// Write queue
		ssOn();
		HAL_SPI_Transmit_IT(&hspi1, data_p, numBytes);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{

		}
		ssOff();

		// Read queue
		sendBuffer[0] = data_p[0] + 1;	// Read address is one greater than the write address
		ssOn();
		HAL_SPI_TransmitReceive_IT(&hspi1, sendBuffer, recvBuffer, numBytes);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur in loadAndVerifyTxQueue\n");
		}
		ssOff();

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
bool readNextSpiMessage(uint8_t *data_p, uint32_t numBytesToRead)
{
	int arraySize = numBytesToRead + 1;	// Array needs to have space for command
	uint8_t sendBuffer[arraySize];
	memset(sendBuffer, 0, arraySize * sizeof(uint8_t));

	// Read numBytesToRead + 1 since we also need to send CMD_RD_NXT_MSG
	sendBuffer[0] = CMD_RD_NXT_MSG;
	ssOn();
	HAL_SPI_TransmitReceive_IT(	&hspi1,
								sendBuffer,
								data_p,
								numBytesToRead + 1);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur while reading next SPI message\n");
		return false;
	}
	ssOff();
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
	uint32_t numDataBytes = 8;		// Number of bytes to be send for writeAll command
	uint8_t recvBuffer[numDataBytes];
	memset(recvBuffer, 0, numDataBytes * sizeof(uint8_t));
	uint8_t sendBuffer[numDataBytes];
	memset(sendBuffer, 0, numDataBytes * sizeof(uint8_t));

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool writeAllSuccess = true;

		sendBuffer[0] = CMD_WR_LD_Q_L0;	// Command for ASCI
		sendBuffer[1] = 0x06;			// Data length for ASCI
		sendBuffer[2] = CMD_WRITE_ALL;	// Command byte for BMBs
		sendBuffer[3] = address;		// Register Address for BMBs
		sendBuffer[4] = (uint8_t)(value & 0x00FF);	// LSB
		sendBuffer[5] = (uint8_t)(value >> 8);		// MSB

		const uint8_t crc = calcCrc(&sendBuffer[2], 4);	// Calculate CRC on data after CMD
		sendBuffer[6] = crc;			// PEC byte
		sendBuffer[7] = 0x00;			// Alive counter seed value for BMBs

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(sendBuffer, numDataBytes))
		{
			break;
		}

		writeRxIntStop(true);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during WRITEALL\n");
			break;
		}

		// Read next SPI message. We don't need the first two bytes (CMD_WR_LD_Q and data length)
		readNextSpiMessage(recvBuffer, numDataBytes-2);

		if (rxErrorsExist())
		{
			break;
		}

		// Verify that the command we sent out matches the command we received
		// We don't need CMD_WR_LD_Q, data length and alive counter
		writeAllSuccess &= !(bool)memcmp(&sendBuffer[2], &recvBuffer[1], numDataBytes - 3);

		// Verify the alive counter
		writeAllSuccess &= (bool)(recvBuffer[6] == numBmbs);

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
	uint32_t numDataBytes = 8;		// Number of bytes to be send for writeDevice command
	uint8_t recvBuffer[numDataBytes];
	memset(recvBuffer, 0, numDataBytes * sizeof(uint8_t));
	uint8_t sendBuffer[numDataBytes];
	memset(sendBuffer, 0, numDataBytes * sizeof(uint8_t));

	uint8_t bmbAddress = (bmbIndex << 3) | 0b100;

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool writeDeviceSuccess = true;

		sendBuffer[0] = CMD_WR_LD_Q_L0;	// Command for ASCI
		sendBuffer[1] = 0x06;			// Data length for ASCI
		sendBuffer[2] = bmbAddress;		// Command byte for BMBs
		sendBuffer[3] = address;		// Register address for BMBs
		sendBuffer[4] = (uint8_t)(value & 0x00FF);	// LSB
		sendBuffer[5] = (uint8_t)(value >> 8);		// MSB

		const uint8_t crc = calcCrc(&sendBuffer[2], 4);	// Calculate CRC on data after CMD
		sendBuffer[6] = crc;			// PEC byte
		sendBuffer[7] = 0x00;			// Alive counter seed value

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(sendBuffer, numDataBytes))
		{
			break;
		}

		writeRxIntStop(true);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during WRITEDEVICE\n");
			break;
		}

		// Read next SPI message. We don't need the first two bytes (CMD_WR_LD_Q and data length)
		readNextSpiMessage(recvBuffer, numDataBytes-2);

		if (rxErrorsExist())
		{
			break;
		}

		// Verify that the command we sent out matches the command we received
		// We don't need CMD_WR_LD_Q, data length and alive counter
		writeDeviceSuccess &= !(bool)memcmp(&sendBuffer[2], &recvBuffer[1], numDataBytes - 3);

		// Verify the alive counter incremented by target device
		writeDeviceSuccess &= (bool)(recvBuffer[6] == 1);

		if (writeDeviceSuccess)
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
	uint32_t numDataBytes = 7;
	uint8_t sendBuffer[numDataBytes];
	memset(sendBuffer, 0, numDataBytes * sizeof(uint8_t));

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readAllSuccess = true;
		sendBuffer[0] = CMD_WR_LD_Q_L0;			// Command for ASCI
		sendBuffer[1] = 0x05 + (numBmbs * 2);	// Data length for ASCI
		sendBuffer[2] = CMD_READ_ALL;			// Command byte for BMBs
		sendBuffer[3] = address;				// Register address for BMBs
		sendBuffer[4] = 0x00;					// Data check byte

		const uint8_t crc = calcCrc(&sendBuffer[2], 3); // Calculate CRC on data after CMD
		sendBuffer[5] = crc;					// PEC byte
		sendBuffer[6] = 0x00;					// Alive counter seed value

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(sendBuffer, 7))
		{
			break;
		}

		writeRxIntStop(true);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during READALL\n");
			break;
		}

		uint32_t numBytesToRead = 5 + (2 * numBmbs); // 5 command bytes + 2 per BMB
		readNextSpiMessage(data_p, numBytesToRead);

		if (rxErrorsExist())
		{
			break;
		}

		// Calculate CRC code based on received data
		const uint8_t calculatedCrc = calcCrc(&data_p[1], 3 + (2 * numBmbs));
		uint8_t recvCrc = data_p[4 + (2 * numBmbs)];

		// Verify data CRC
		readAllSuccess &= (calculatedCrc == recvCrc);
		// Verify Alive-counter byte
		readAllSuccess &= (data_p[5 + (2 * numBmbs)] == numBmbs);

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
	uint32_t numDataBytes = 7;
	uint8_t sendBuffer[numDataBytes];
	memset(sendBuffer, 0, numDataBytes * sizeof(uint8_t));

	uint8_t bmbAddress = (bmbIndex << 3) | 0b101;

	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readDeviceSuccess = true;
		sendBuffer[0] = CMD_WR_LD_Q_L0;	// Command for ASCI
		sendBuffer[1] = 0x07;			// Data length for ASCI
		sendBuffer[2] = bmbAddress;		// Command byte for BMBs
		sendBuffer[3] = address;		// Register address for BMBs
		sendBuffer[4] = 0x00;			// Data check byte

		const uint8_t crc = calcCrc(&sendBuffer[2], 3); // Calculate CRC on data after CMD
		sendBuffer[5] = crc;			// PEC byte
		sendBuffer[6] = 0x00;			// Alive counter seed value

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(sendBuffer, 7))
		{
			break;
		}

		writeRxIntStop(true);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during READALL\n");
			break;
		}

		uint32_t numBytesToRead = 7; // 5 command bytes + 2 data bytes
		readNextSpiMessage(data_p, numBytesToRead);

		if (rxErrorsExist())
		{
			break;
		}

		// Calculate CRC code based on received data
		const uint8_t calculatedCrc = calcCrc(&data_p[1], 5);
		uint8_t recvCrc = data_p[6];

		// Verify data CRC
		readDeviceSuccess &= (calculatedCrc == recvCrc);

		// Verify Alive-counter byte
		readDeviceSuccess &= (data_p[7] == 1);

		if (readDeviceSuccess)
		{
			return true;
		}
	}
	printf("Failed to READDEVICE\n");
	return false;
}
