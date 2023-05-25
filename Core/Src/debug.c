/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "debug.h"
#include "bms.h"
#include "leakyBucket.h"
#include "alerts.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define DEBUG_UPDATE_PERIOD_MS		1000

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern LeakyBucket_S asciCommsLeakyBucket;
extern Bms_S gBms;

/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

static void printBalancingStatus();
static void printCellVoltages();
static void printCellTemperatures();
static void printInternalResistances();
static void printBoardTemperatures();
static void printActiveAlerts();
static void printLeakyBucket();
static void printTractiveCurrent();
// TODO add soc

/* ==================================================================== */
/* ==================== LOCAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

static void printLeakyBucket()
{
	printf("Leaky Bucket Filled: ");
	leakyBucketFilled(&asciCommsLeakyBucket) ? printf("TRUE\n") : printf("FALSE\n");
}

static void printTractiveCurrent()
{
	printf("Tractive Current: %6.3f\n", (double)gBms.tractiveSystemCurrent);
}

static void printBalancingStatus()
{
	printf("Balancing Enabled: ");
	gBms.packBalancing ? printf("TRUE\n") : printf("FALSE\n");
}

static void printCellVoltages()
{
	printf("Cell Voltage:\n");
	printf("Max: %5.3fV\t Min: %5.3fV\t Avg: %5.3fV\n", (double)gBms.maxBrickV, (double)gBms.minBrickV, (double)gBms.avgBrickV);
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    | Segment |\n");
	for (int32_t i = 0; i < gBms.numBmbs; i++)
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

static void printCellTemperatures()
{
	printf("Cell Temp:\n");
	printf("Max: %5.1fC\t Min: %5.1fC\t Avg: %5.1fC\n", (double)gBms.maxBrickTemp, (double)gBms.minBrickTemp, (double)gBms.avgBrickTemp);
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |\n");
	for (int32_t i = 0; i < gBms.numBmbs; i++)
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

static void printInternalResistances()
{
	printf("Internal Resistance:\n");
	printf("|   BMB   |    1    |    2    |    3    |    4    |    5    |    6    |    7    |    8    |    9    |   10    |   11    |   12    |\n");
	for (int32_t i = 0; i < gBms.numBmbs; i++)
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

static void printBoardTemperatures()
{
	printf("Board Temp:\n");
	printf("Max: %5.1fC\t Min: %5.1fC\t Avg: %5.1fC\n", (double)gBms.maxBoardTemp, (double)gBms.minBoardTemp, (double)gBms.avgBoardTemp);
	printf("|   BMB   |    1    |    2    |    3    |    4    |\n");
	for (int32_t i = 0; i < gBms.numBmbs; i++)
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

static void printActiveAlerts()
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

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void printBmsStruct()
{
    static uint32_t lastUpdateDebug = 0;
	if((HAL_GetTick() - lastUpdateDebug) >= DEBUG_UPDATE_PERIOD_MS)
	{
        // Clear console
		printf("\e[1;1H\e[2J");
        printBalancingStatus();
        printCellVoltages();
        printCellTemperatures();
        // printInternalResistances(&gBms);
        printBoardTemperatures();
        printActiveAlerts();
        printLeakyBucket();
        printTractiveCurrent();

        lastUpdateDebug = HAL_GetTick();
    }
}
