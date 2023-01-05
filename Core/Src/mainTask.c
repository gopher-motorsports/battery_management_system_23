/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <bms.h>
#include "main.h"
#include "cmsis_os.h"
#include "mainTask.h"
#include "bmbInterface.h"
#include "bmb.h"


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
extern osSemaphoreId 		binSemHandle;
extern SPI_HandleTypeDef 	hspi1;
extern Bms_S 				gBms;


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */
uint32_t numBmbs = 0;
static bool initialized = false;
static uint32_t initRetries = 5;
static uint32_t lastUpdateMain = 0;

static bool balancingEnabled = false;
static uint32_t lastBalancingUpdate = 0;


/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */
void printCellVoltages();
void printCellTemperatures();
void printBoardTemperatures();





void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_8)
	{
		static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xSemaphoreGiveFromISR(binSemHandle, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	if (GPIO_Pin == B1_Pin)
	{
		if (HAL_GetTick() - lastBalancingUpdate > 300)
		{
			lastBalancingUpdate = HAL_GetTick();
			balancingEnabled = !balancingEnabled;
		}
	}
}
/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */
void runMain()
{
	if (!initialized && initRetries > 0)
	{
		printf("Initializing ASCI connection...\n");
		resetASCI();
		initASCI();
		initialized = helloAll(&numBmbs);
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

		aggregateBrickVoltages(gBms.bmb,numBmbs);

		if((HAL_GetTick() - lastUpdateMain) >= 1000)
		{

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

			printCellVoltages();
			printCellTemperatures();
			printBoardTemperatures();

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
	for (int i = 0; i < numBmbs; i++)
	{
		printf("|    %02d   |", i + 1);
		for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
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
}

void printBoardTemperatures()
{
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



