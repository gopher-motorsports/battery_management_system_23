/*
 * spiUtils.c
 *
 *  Created on: Aug 6, 2022
 *      Author: sebas
 */

#include "main.h"
#include "cmsis_os.h"

#include "spiUtils.h"

extern osSemaphoreId binSemHandle;
extern SPI_HandleTypeDef hspi1;
extern uint8_t spiRecvBuffer[SPI_BUFF_SIZE];
extern uint8_t spiSendBuffer[SPI_BUFF_SIZE];


void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi1)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi == &hspi1)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_8)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
}

void ssOn()
{
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_RESET);
}
void ssOff()
{
	HAL_GPIO_WritePin(SS_GPIO_Port, SS_Pin, GPIO_PIN_SET);
}

void enableASCI()
{
	HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_SET);
}
void disableASCI()
{
	HAL_GPIO_WritePin(SHDN_GPIO_Port, SHDN_Pin, GPIO_PIN_RESET);
}
void resetASCI()
{
	disableASCI();
	vTaskDelay(10);
	enableASCI();
}

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
void sendSPI(uint8_t value)
{
	ssOn();
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&value, 1);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("FUCK\n");
	}
	ssOff();
}
void clearRxBuffer()
{
	sendSPI(CMD_CLR_RX_BUF);
}
void clearTxBuffer()
{
	sendSPI(CMD_CLR_TX_BUF);
}
bool clearRxIntFlags()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, 0x00);
	uint8_t result = readRegister(R_RX_INTERRUPT_FLAGS);
	return ((result & ~(0x40)) == 0x00);
}
bool clearTxIntFlags()
{
	return writeAndVerifyRegister(R_TX_INTERRUPT_FLAGS, 0x00);
}
bool writeRxIntStop(Bit_Status_E bitStatus)
{
	return writeRegisterBit(R_RX_INTERRUPT_ENABLE, 1U, bitStatus);
}

bool writeRegisterBit(uint8_t registerAddress, uint8_t bitNumber, Bit_Status_E bitStatus)
{
	if (bitNumber >= 8) return false;	// Invalid input

	const uint8_t bitMask = (0x01 << bitNumber);
	const uint8_t regData = readRegister(registerAddress);

	if (bitStatus == BIT_SET)
	{
		writeRegister(registerAddress, (regData | bitMask));
	}
	else if (bitStatus == BIT_CLEAR)
	{
		writeRegister(registerAddress, (regData & (~bitMask)));
	}

	if ((readRegister(registerAddress) & bitMask) == (uint8_t)bitStatus)
	{
		return true;
	}
	else
	{
		return false;
	}
}

uint8_t readRegister(uint8_t registerAddress)
{
	ssOn();
	// Since reading add 1 to address
	const uint8_t sendBuffer[2] = {registerAddress + 1};
	uint8_t recvBuffer[2] = {0};
	HAL_SPI_TransmitReceive_IT(&hspi1, (uint8_t *)&sendBuffer, (uint8_t *)&recvBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		// TODO figure this out
		printf("FUCK\n");
	}
	ssOff();
	return recvBuffer[1];
}
void writeRegister(uint8_t registerAddress, uint8_t value)
{
	ssOn();
	uint8_t sendBuffer[2] = {registerAddress, value};
	HAL_SPI_Transmit_IT(&hspi1, (uint8_t *)&sendBuffer, 2);
	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("FUCK\n");
	}
	ssOff();
}

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

bool readRxBusyFlag()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x20)
	{
		return true;
	}
	return false;
}

void clearRxBusyFlag()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x20));
}

bool readRxStopFlag()
{
	uint8_t rxIntFlags = readRegister(R_RX_INTERRUPT_FLAGS);
	if (rxIntFlags & 0x02)
	{
		return true;
	}
	return false;
}

void clearRxStopFlag()
{
	writeRegister(R_RX_INTERRUPT_FLAGS, ~(0x02));
}


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

bool loadAndVerifyTxQueue(uint8_t *data_p, uint32_t numBytes)
{
	bool queueDataVerified = false;
	uint8_t sendBuffer[SPI_BUFF_SIZE] = { 0 };
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
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
		HAL_SPI_TransmitReceive_IT(&hspi1, sendBuffer, (uint8_t *)&spiRecvBuffer, numBytes);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur in loadAndVerifyTxQueue\n");
		}
		ssOff();

		// Verify data in load queue
		queueDataVerified = !(bool)memcmp(&data_p[1], &spiRecvBuffer[1], numBytes - 1);
		if (queueDataVerified) { return true; }
	}
	printf("Failed to load and verify TX queue\n");
	return false;
}

bool readNextSpiMessage(uint8_t *data_p, uint32_t numBytesToRead)
{
	if (numBytesToRead > SPI_BUFF_SIZE)
	{
		printf("Trying to read more bytes than buffer allocated for!\n");
		numBytesToRead = SPI_BUFF_SIZE-1;
	}
	// Read numBytesToRead + 1 since we also need to send CMD_RD_NXT_MSG
	spiSendBuffer[0] = CMD_RD_NXT_MSG;
	ssOn();
	HAL_SPI_TransmitReceive_IT(	&hspi1,
								(uint8_t *)&spiSendBuffer,
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

bool writeAll(uint8_t address, uint16_t value, uint8_t *data_p, uint32_t numBmbs)
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool writeAllSuccess = true;
		uint32_t numDataBytes = 8;

		spiSendBuffer[0] = CMD_WR_LD_Q_L0;
		spiSendBuffer[1] = 0x06;			// Data length
		spiSendBuffer[2] = CMD_WRITE_ALL;
		spiSendBuffer[3] = address;
		spiSendBuffer[4] = (uint8_t)(value & 0x00FF);	// LSB
		spiSendBuffer[5] = (uint8_t)(value >> 8);		// MSB

		const uint8_t crc = calcCrc(&spiSendBuffer[2], 4);
		spiSendBuffer[6] = crc;
		spiSendBuffer[7] = 0x00;			// Alive counter seed value

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(spiSendBuffer, numDataBytes))
		{
			break;
		}

		writeRxIntStop(BIT_SET);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during WRITEALL\n");
			break;
		}

		// Read next SPI message. We don't need the first two bytes (CMD_WR_LD_Q and data length)
		readNextSpiMessage(spiRecvBuffer, numDataBytes-2);

		if (rxErrorsExist())
		{
			break;
		}

		// Verify that the command we sent out matches the command we received
		writeAllSuccess &= !(bool)memcmp(&spiSendBuffer[2], &spiRecvBuffer[1], numDataBytes - 3);

		// Verify the alive counter
		writeAllSuccess &= (bool)(spiRecvBuffer[6] == numBmbs);

		if (writeAllSuccess)
		{
			return true;
		}
	}
	printf("Failed to write all\n");
	return false;
}

bool readAll(uint8_t address, uint32_t numBmbs, uint8_t *data_p)
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		bool readAllSuccess = true;
		spiSendBuffer[0] = CMD_WR_LD_Q_L0;
		spiSendBuffer[1] = 0x05 + 0x2 * numBmbs;	// Data length
		spiSendBuffer[2] = CMD_READ_ALL;
		spiSendBuffer[3] = address;
		spiSendBuffer[4] = 0x00;					// Data check byte

		const uint8_t crc = calcCrc(&spiSendBuffer[2], 3);
		spiSendBuffer[5] = crc;						// PEC byte
		spiSendBuffer[6] = 0x00;					// Alive counter seed value

		// Send command to ASCI and verify data integrity
		if(!loadAndVerifyTxQueue(spiSendBuffer, 7))
		{
			break;
		}

		writeRxIntStop(BIT_SET);
		clearRxIntFlags();

		sendSPI(CMD_WR_NXT_LD_Q_L0);
		if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
		{
			printf("Interrupt failed to occur during READALL\n");
			break;
		}

		uint32_t numBytesToRead = 5 + (2 * numBmbs);
		readNextSpiMessage(spiRecvBuffer, numBytesToRead);

		if (rxErrorsExist())
		{
			break;
		}

		const uint8_t calculatedCrc = calcCrc(&spiRecvBuffer[1], 3 + (2 * numBmbs));
		uint8_t recvCrc = spiRecvBuffer[4 + (2 * numBmbs)];

		// Verify data CRC
		readAllSuccess &= (calculatedCrc == recvCrc);
		// Verify Alive-counter byte
		readAllSuccess &= (spiRecvBuffer[5 + (2 * numBmbs)] == numBmbs);

		if (readAllSuccess)
		{
			return true;
		}
	}
	printf("Failed to READALL\n");
	return false;
}
