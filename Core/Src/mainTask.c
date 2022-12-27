#include <bms.h>
#include "main.h"
#include "cmsis_os.h"

#include "mainTask.h"
#include "spiUtils.h"
#include "bmb.h"

extern osSemaphoreId 		binSemHandle;
extern SPI_HandleTypeDef 	hspi1;
extern Bms_S 				gBms;

uint32_t numBmbs = 0;
static bool initialized = false;
static uint32_t initRetries = 5;
static uint32_t lastUpdateMain = 0;
void runMain()
{
	if (!initialized && initRetries > 0)
	{
		printf("Initializing ASCI connection...\n");
		resetASCI();
		initialized = initASCI(&numBmbs);

		initBatteryPack(numBmbs);
		printf("Number of BMBs detected: %lu\n", numBmbs);

		initRetries--;
	}
	else if(initialized)
	{
		// TODO verify number of BMBs
		writeAll(ACQCFG, 0xFFFF, numBmbs);
		// set THRM manual ON

//		updateBmbTempData(gBms.bmb,numBmbs);
		updateBmbData(gBms.bmb, numBmbs);
		aggregateBrickVoltages(gBms.bmb,numBmbs);

		if((HAL_GetTick() - lastUpdateMain) >= 1000)
		{
			// Update lastUpdate
			lastUpdateMain = HAL_GetTick();

			printf("Cell Voltage:\n");
			printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    | StackV  |\n");
			for (int i = 0; i < numBmbs; i++)
			{
				printf("|    %02d   |", i + 1);
				for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
				{
					printf("  %5.3f  |", gBms.bmb[i].brickV[j]);
				}
				printf("  %5.2f  |", gBms.bmb[i].stackV);
				printf("\n");
			}
			printf("\n");
			printf("Cell Temp:\n");
			printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |\n");
			for (int i = 0; i < numBmbs; i++)
			{
				printf("|    %02d   |", i + 1);
				for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
				{
					printf(" %5.1fC  |", gBms.bmb[i].brickTemp[j]);
				}
				printf("\n");
			}
			printf("\n");
			printf("Board Temp:\n");
			printf("|   BMB   |    1    |    2    |    3    |    4    |\n");
			for (int i = 0; i < numBmbs; i++)
			{
				printf("|    %02d   |", i + 1);
				for (int j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
				{
					printf(" %5.1fC  |", gBms.bmb[i].boardTemp[j]);
				}
				printf("\n");
			}
			printf("\n");
		}

	}
	else
	{
		printf("Failed to initialize");
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



