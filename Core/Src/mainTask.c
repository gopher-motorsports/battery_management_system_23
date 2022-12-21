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
#include "bmb.h"
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
			// TODO verify number of BMBs
			writeAll(ACQCFG, 0xFFFF, spiRecvBuffer, numBmbs);	// set THRM manual ON

			updateBmbData(numBmbs);
			aggregateBrickVoltages(numBmbs);
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
			readBoardTemps(numBmbs);
			for (int i = 0; i < numBmbs; i++)
			{
				for (int j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
				{
					printf("%d - %1.3f\n", j, gPack.bmb[i].boardTempVoltage[j]);
				}
			}

		}

	}

	// New functions to add UpdateBrickVoltages() - reads in all brick voltages
	// Read Stack Voltages  - reads all stack voltages
	// Aggregate brick voltages - get min and max brick and avg from each bmb
	// Aggregate stack voltages -  get min max and avg from each
	// compare brickV vs stack v and see if there are errors
	// figure out how to measure external inputs
	// Toggle the gpios
	// Create cell config data - max voltage min voltage



}



