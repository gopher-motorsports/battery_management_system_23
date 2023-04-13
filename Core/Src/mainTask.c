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
#include "debug.h"
#include <stdlib.h>
#include "GopherCAN_network.h"
#include "internalResistance.h"


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern SPI_HandleTypeDef 	hspi1;
extern SPI_HandleTypeDef 	hspi2;
extern Bms_S 				gBms;

extern LeakyBucket_S asciCommsLeakyBucket;



/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

uint32_t numBmbs = 0;
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
void printInternalResistances();
void printActiveAlerts();


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void initMain()
{
	HAL_GPIO_WritePin(AMS_FAULT_OUT_GPIO_Port, AMS_FAULT_OUT_Pin, GPIO_PIN_SET);
	for (int i = 0; i < initRetries; i++)
	{
		// Try to initialize the BMS HW
		if (initBatteryPack(&numBmbs))
		{
			// Successfully initialized
			return;
		}
	}
	Debug("Failed to initialize BMS\n");
}

void runMain()
{
	if (gBms.bmsHwState == BMS_BMB_FAILURE)
	{
		// Retry initializing the BMBs
		initBatteryPack(&numBmbs);
	}
	else
	{
		updateTractiveCurrent();
		
		updatePackData(numBmbs);

		updateGopherCan();

		updateImdStatus();

		updateEpaper();

		checkAndHandleAlerts();

		if((HAL_GetTick() - lastUpdateMain) >= 1000)
		{
			if (leakyBucketFilled(&asciCommsLeakyBucket))
			{
				gBms.bmsHwState = BMS_BMB_FAILURE;
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
			// balancePackToVoltage(numBmbs, 3.87f);

			printCellVoltages();
			printCellTemperatures();
			printInternalResistances();
			printBoardTemperatures();
			printActiveAlerts();

			printf("Leaky bucket filled: %d\n\n", leakyBucketFilled(&asciCommsLeakyBucket));

			printf("Tractive Current: %6.3f\n", (double)gBms.tractiveSystemCurrent);

			// Update lastUpdate
			lastUpdateMain = HAL_GetTick();
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

void printCellVoltages()
{
	printf("Cell Voltage:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    | Segment |\n");
	for (int32_t i = 0; i < numBmbs; i++)
	{
		printf("|    %02ld   |", i + 1);
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			printf("  %5.3f", (double)gBms.bmb[i].brickV[j]);
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
		printf("  %5.2f  |", (double)gBms.bmb[i].segmentV);
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
			printf(" %5.1fC  |", (double)gBms.bmb[i].brickTemp[j]);
		}
		printf("\n");
	}
	printf("\n");
}

void printInternalResistances()
{
	printf("Internal Resistance:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |\n");
	for (int32_t i = 0; i < numBmbs; i++)
	{
		printf("|    %02ld   |", i + 1);
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			printf("%5.1fmOhm|", (double)(gBms.bmb[i].brickResistance[j] * 1000.0f));
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
			printf(" %5.1fC  |", (double)gBms.bmb[i].boardTemp[j]);
		}
		printf("\n");
	}
	printf("\n");
}

void printActiveAlerts()
{
	printf("Alerts Active:\n");
	uint32_t numActiveAlerts = 0;
	for (uint32_t i = 0; i < NUM_ALERTS; i++)
	{
		Alert_S* alert = alerts[i];
		if (getAlertStatus(alert) == ALERT_SET)
		{
			printf("%s - ACTIVE!\n", alert->alertName);
			numActiveAlerts++;
		}
	}
	if (numActiveAlerts == 0)
	{
		printf("None\n");
	}
	printf("\n");
}



