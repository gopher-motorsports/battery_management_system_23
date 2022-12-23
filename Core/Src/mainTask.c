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
void runMain()
{
	if (!initialized && initRetries > 0)
	{
		printf("Initializing ASCI connection...\n");
		resetASCI();
		initialized = initASCI(&numBmbs);
		initRetries--;
	}
	else if(initialized)
	{
		printf("Number of BMBs detected: %lu\n", numBmbs);
		initBatteryPack(numBmbs);
		// TODO verify number of BMBs
		writeAll(ACQCFG, 0xFFFF, numBmbs);	// set THRM manual ON

		cyclicUpdateBmbData(gBms.bmb, numBmbs);
		aggregateBrickVoltages(gBms.bmb,numBmbs);

		printf("|  BMB  | StackV |   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |  10   |  11   |  12   |\n");
		for (int i = 0; i < numBmbs; i++)
		{
			printf("|   %02d  |  %05.2f |", i + 1, gBms.bmb[i].stackV);
			for (int j = 0; j < 12; j++)
			{
				printf(" %1.3f |", gBms.bmb[i].brickV[j]);
			}
			printf("\n");
		}
		for (int i = 0; i < numBmbs; i++)
		{
			for (int j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
			{
				printf("Board Temps: \n");
				printf("%d - %1.3fC\n", j, gBms.bmb[i].boardTemp[j]);
				printf("\n");
			}
			for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
			{
				printf("Cell Temps: \n");
				printf("%d - %1.3fC\n", j, gBms.bmb[i].brickTemp[j]);
				printf("\n");
			}
		}
	}
	else {
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



