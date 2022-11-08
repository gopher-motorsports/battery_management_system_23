/*
 * mainTask.c
 *
 *  Created on: Aug 6, 2022
 *      Author: sebas
 */
#include "main.h"
#include "cmsis_os.h"

#include "mainTask.h"
#include "spiUtils.h"
#include "batteryPack.h"

extern osSemaphoreId 		binSemHandle;
extern SPI_HandleTypeDef 	hspi1;
extern Pack_S 				gPack;

uint8_t spiSendBuffer[SPI_BUFF_SIZE];
uint8_t spiRecvBuffer[SPI_BUFF_SIZE];


uint32_t numBmbs = 0;
static bool initialized = false;
static uint32_t initRetries = 5;
void runMain()
{
	if (!initialized && initRetries > 0)
	{
		printf("Initializing ASCI connection...\n");
		resetASCI();
		initialized = initASCI(&numBmbs);
		initRetries--;
		if (initialized)
		{
			printf("Number of BMBs detected: %lu\n", numBmbs);
			initBatteryPack(numBmbs);
//			updateBmbData();
//			aggregateBrickVoltages();
//			readRegister(R_CONFIG_3);
			writeAll(0x13, 0x0001, spiRecvBuffer, numBmbs);
			readAll(0x2C, 1U, spiRecvBuffer);
			for (int i = 0; i < numBmbs; i++)
			{
				const uint32_t stackVRaw = ((spiRecvBuffer[4+(2*i)] << 8) | spiRecvBuffer[3+(2*i)]) >> 2;
				const float stackV = (stackVRaw * 362U) / 100000.0f;
				gPack.bmb[i].stackV = stackV;
			}
			// Iterate through all of the cells
			for (uint8_t i = 0; i < 12; i++)
			{
				uint8_t cellReg = i + 0x20;
				// TODO: add catch if readAll fails
				readAll(cellReg, 1U, spiRecvBuffer);
				// Iterate through received data and update
				for (int j = 0; j < numBmbs; j++)
				{
					const uint32_t brickVRaw = ((spiRecvBuffer[4+(2*j)] << 8) | spiRecvBuffer[3+(2*j)]) >> 2;
					// 5V range & 14 bit ADC - 5/(2^14) = 305.176 uV/bit
					const float brickV = brickVRaw * 0.000305176f;
					gPack.bmb[j].brickV[i] = brickV;
				}
			}
			printf("|  BMB  | StackV |   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |  10   |  11   |  12   |\n");
			for (int i = 0; i < numBmbs; i++)
			{
				printf("|   %02d  |  %05.2f |", i + 1, gPack.bmb[i].stackV);
				for (int j = 0; j < 12; j++)
				{
					printf(" %1.3f |", gPack.bmb[i].brickV[j]);
				}
				printf("\n");
			}

		}

	}

	// Toggle pins
//	vTaskDelay(3000);
//	writeAll(0x11, 0xF00F, spiRecvBuffer);
//	printf("HIGH\n");
//
//	vTaskDelay(3000);
//	writeAll(0x11, 0xF000, spiRecvBuffer);
//	printf("LOW\n");






	// Todo verify that numBmbs is as expected
//	writeAll(0x13, 0x0001, spiRecvBuffer);
//	readAll(0x2C, 1U, spiRecvBuffer);
//	uint32_t stackVRaw = ((spiRecvBuffer[4] << 8) | spiRecvBuffer[3]) >> 2;
//	float stackV = (stackVRaw * 362U) / 100000.0f;
//	for (uint8_t i = 0; i < 12; i++)
//	{
//		uint8_t cellReg = i + 0x20;
//		readAll(cellReg, 1U, spiRecvBuffer);
//		uint32_t brickVRaw = ((spiRecvBuffer[4] << 8) | spiRecvBuffer[3]) >> 2;
//		// 5V range & 14 bit ADC - 5/(2^14) = 305.176 uV/bit
//		float brickV = brickVRaw * 0.000305176f;
//		printf("B%d: \t%f\n", i, brickV);
//	}




	// Read device config
//	readAll(0x00, 1U, spiRecvBuffer);
//
//
//	printf("Starting brick voltage acquisition:\n");
//	// Read cell
//	readAll(0x21, 1U, spiRecvBuffer);
//	// read bulk
//	readAll(0x2C, 1U, spiRecvBuffer);
//
//	for (uint8_t i = 0; i < 12; i++)
//	{
//		uint8_t cellReg = i + 0x20;
//		readAll(cellReg, 1U, spiRecvBuffer);
//		uint32_t brickVRaw = ((spiRecvBuffer[4] << 8) | spiRecvBuffer[3]) >> 2;
//		// 5V range & 14 bit ADC - 5/(2^14) = 305.176 uV/bit
//		float brickV = brickVRaw * 0.000305176f;
//		printf("B%d: \t%.4gV\n", i, brickV);
//	}


	// New functions to add UpdateBrickVoltages() - reads in all brick voltages
	// Read Stack Voltages  - reads all stack voltages
	// Aggregate brick voltages - get min and max brick and avg from each bmb
	// Aggregate stack voltages -  get min max and avg from each
	// compare brickV vs stack v and see if there are errors
	// figure out how to measure external inputs
	// Toggle the gpios
	// Create cell config data - max voltage min voltage



}



