/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "main.h"
#include "cmsis_os.h"
#include "mainTask.h"
#include "bms.h"
#include "bmbInterface.h"
#include "bmbUtils.h"
#include "bmb.h"
#include "epaper.h"
#include "epaperUtils.h"
#include <stdlib.h>
#include "GopherCAN_network.h"


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern osSemaphoreId 		asciSpiSemHandle;
extern osSemaphoreId 		asciSemHandle;
extern osSemaphoreId 		epdSemHandle;
extern osSemaphoreId		epdBusySemHandle;
extern SPI_HandleTypeDef 	hspi1;
extern SPI_HandleTypeDef 	hspi2;
extern Bms_S 				gBms;

extern LeakyBucket_S asciCommsLeakyBucket;



/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

uint32_t numBmbs = 0;
static bool initialized = false;
static uint32_t initRetries = 5;
static uint32_t lastUpdateMain = 0;

bool balancingEnabled = false;
uint32_t lastBalancingUpdate = 0;

/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

void printCellVoltages();
void printCellTemperatures();
void printBoardTemperatures();


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void runMain()
{
	// float value = bmsTractiveSystemCurrentLow_A.data;
	// float value2 = bmsTractiveSystemCurrentHigh_A.data;
	// printf("CSNS: %f\n", value);
	// printf("CSNS: %f\n", value*2);

	if (!initialized && initRetries > 0)
	{
		initialized = true;
		printf("Initializing ASCI connection...\n");
		resetASCI();
		initialized &= initASCI();
		initialized &= helloAll(&numBmbs);
		// initialized = initASCI(&numBmbs);
		if (numBmbs != NUM_BMBS_PER_PACK)
		{
			printf("Number of BMBs detected (%lu) doesn't match expectation (%d)\n", numBmbs, NUM_BMBS_PER_PACK);
			initialized = false;
		}

		initBatteryPack(numBmbs);
		printf("Number of BMBs detected: %lu\n", numBmbs);

		initRetries--;
	}
	else if(initialized)
	{

		updateBmbData(gBms.bmb, numBmbs);

		aggregatePackData(numBmbs);

		updateImdStatus();

		updateSdcStatus();

		updateEpaper();

		if((HAL_GetTick() - lastUpdateMain) >= 1000)
		{
			if (leakyBucketFilled(&asciCommsLeakyBucket))
			{
				// Try to reset the asci to re-establish comms
				resetASCI();
				initASCI();
				helloAll(&numBmbs);
				initBatteryPack(numBmbs);
			}
			// Clear console
			printf("\e[1;1H\e[2J");

			if(balancingEnabled)
			{
				printf("Balancing Enabled: TRUE\n");
			}
			else
			{
				printf("Balancing Enabled: FALSE\n");
			}
			balancePack(numBmbs, balancingEnabled);
			// balancePackToVoltage(numBmbs, 3.7f);

			printCellVoltages();
			printCellTemperatures();
			printBoardTemperatures();
			printf("Leaky bucket filled: %d\n", leakyBucketFilled(&asciCommsLeakyBucket));

			// Update lastUpdate
			lastUpdateMain = HAL_GetTick();
		}

	}
	else
	{
		printf("Failed to initialize\n");
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

void printCellVoltages()
{
	printf("Cell Voltage:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    | StackV  |\n");
	for (int32_t i = 0; i < numBmbs; i++)
	{
		printf("|    %02ld   |", i + 1);
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			printf("  %5.3f", gBms.bmb[i].brickV[j]);
			if(gBms.bmb[i].balSwEnabled[j])
			{
				printf("*");
			}
			else
			{
				printf(" ");
			}
			printf(" |");
		}
		printf("  %5.2f  |", gBms.bmb[i].stackV);
		printf("\n");
	}
	printf("\n");
}

void printCellTemperatures()
{
	printf("Cell Temp:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |\n");
	for (int32_t i = 0; i < numBmbs; i++)
	{
		printf("|    %02ld   |", i + 1);
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			printf(" %5.1fC  |", gBms.bmb[i].brickTemp[j]);
		}
		printf("\n");
	}
	printf("\n");
}

void printBoardTemperatures()
{
	printf("Board Temp:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |\n");
	for (int32_t i = 0; i < numBmbs; i++)
	{
		printf("|    %02ld   |", i + 1);
		for (int32_t j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
		{
			printf(" %5.1fC  |", gBms.bmb[i].boardTemp[j]);
		}
		printf("\n");
	}
	printf("\n");
}



