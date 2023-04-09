/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "cmsis_os.h"
#include "alerts.h"
#include "bms.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "debug.h"
#include "GopherCAN.h"
#include "debug.h"
#include "currentSense.h"
#include "internalResistance.h"
#include "gopher_sense.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define INIT_BMS_BMB_ARRAY \
{ \
    [0 ... NUM_BMBS_IN_ACCUMULATOR-1] = {.bmbIdx = __COUNTER__} \
}

Bms_S gBms = 
{
    .numBmbs = NUM_BMBS_IN_ACCUMULATOR,
    .bmb = INIT_BMS_BMB_ARRAY
};

extern osMessageQId epaperQueueHandle;

extern CAN_HandleTypeDef hcan2;

static uint32_t lastEpapUpdate = 0;

static void disableBmbBalancing(Bmb_S* bmb);

static void disableBmbBalancing(Bmb_S* bmb)
{
	for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
	{
		bmb->balSwRequested[i] = false;
	}
}

void initBmsGopherCan(CAN_HandleTypeDef* hcan)
{
	// initialize CAN
	if (init_can(GCAN0, hcan, BMS_ID, BXTYPE_MASTER))
	{
		gBms.bmsHwState = BMS_GSNS_FAILURE;
	}
}

/*!
  @brief   Initialization function for the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @returns bool True if initialization successful, false otherwise
*/
bool initBatteryPack(uint32_t* numBmbs)
{
	gBms.balancingDisabled = true;
	gBms.emergencyBleed    = false;
	gBms.chargingDisabled  = true;
	gBms.limpModeEnabled   = false;
	gBms.amsFaultPresent   = false;
	
	if (!initASCI())
	{
		goto initializationError;
	}

	if (!helloAll(numBmbs))
	{
		goto initializationError;
	}

	if (*numBmbs != NUM_BMBS_IN_ACCUMULATOR)
	{
		Debug("Number of BMBs detected (%lu) doesn't match expectation (%d)\n", *numBmbs, NUM_BMBS_IN_ACCUMULATOR);
		goto initializationError;
	}

	if (!initBmbs(*numBmbs))
	{
		goto initializationError;
	}

	gBms.numBmbs = *numBmbs;
	gBms.bmsHwState = BMS_NOMINAL;
	return true;

// Routine if initialization error ocurs
initializationError:
	// Set hardware error status
	gBms.bmsHwState = BMS_BMB_FAILURE;

	// Determine if a chain break exists
	initASCI();	// Ignore return value as it will be bad due to no loopback
	helloAll(numBmbs);	// Ignore return value as it will be bad due to no loopback
	uint32_t breakLocation = detectBmbDaisyChainBreak(gBms.bmb, NUM_BMBS_IN_ACCUMULATOR);
	if (breakLocation == 1)
	{
		Debug("BMB Chain Break detected between BMS and BMB 1\n");
	}
	else if (breakLocation != 0)
	{
		// breakLocation is 0 indexed but we should print 1 indexed bmb values
		Debug("BMB Chain Break detected between BMB %lu and BMB %lu\n", breakLocation, breakLocation + 1);
	}
	else
	{
		// Break location is in the external loopback of the final BMB
		// This is fixable by enabling the internal loopback on the final BMB which is 
		// done in the detectBmbDaisyChainBreak function
		if (helloAll(numBmbs))
		{
			// helloAll command succeeded - verify that the numBmbs was correctly set
			if (*numBmbs != NUM_BMBS_IN_ACCUMULATOR)
			{
				return false;
			}
			if (initBmbs(*numBmbs))
			{
				gBms.bmsHwState = BMS_NOMINAL;
				return true;
			}
		}
	}
	return false;
}

void updatePackData(uint32_t numBmbs)
{
	static uint32_t lastPackUpdate = 0;
	if(HAL_GetTick() - lastPackUpdate > VOLTAGE_DATA_UPDATE_PERIOD_MS)
	{
		updateBmbData(gBms.bmb, numBmbs);
		aggregatePackData(numBmbs);
		updateInternalResistanceCalcs(&gBms);
	}
}

/*!
  @brief   Handles balancing the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   balanceRequested - True if we want to balance, false otherwise
*/
void balancePack(uint32_t numBmbs, bool balanceRequested)
{
	// TODO: Determine how we want to handle EMERGENCY_BLEED

	// If balancing not requested or balancing disabled ensure all balance switches off
	if (!balanceRequested || gBms.balancingDisabled)
	{
		for (int32_t i = 0; i < numBmbs; i++)
		{
			disableBmbBalancing(&gBms.bmb[i]);
		}
		balanceCells(gBms.bmb, numBmbs);
		return;
	}

	// Determine minimum voltage across entire battery pack
	float bleedTargetVoltage = gBms.minBrickV;

	// Ensure we don't overbleed the cells
	if (bleedTargetVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		bleedTargetVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}		
	balancePackToVoltage(numBmbs, bleedTargetVoltage);
}

void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage)
{
	// Clamp target brick voltage if too low
	if (targetBrickVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		targetBrickVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}

	// Iterate through all BMBs and set bleed request
	for (int32_t i = 0; i < numBmbs; i++)
	{
		// Iterate through all bricks and determine whether they should be bled or not
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			if (gBms.bmb[i].brickV[j] > targetBrickVoltage + BALANCE_THRESHOLD_V)
			{
				gBms.bmb[i].balSwRequested[j] = true;
			}
			else
			{
				gBms.bmb[i].balSwRequested[j] = false;
			}
		}
	}
	
	balanceCells(gBms.bmb, numBmbs);
}

void checkAndHandleAlerts(uint32_t numBmbs)
{
	// Run each alert monitor
	for (uint32_t i = 0; i < NUM_ALERTS; i++)
	{
		runAlertMonitor(&gBms, alerts[i]);
	}

	// Accumulate alert statuses
	bool responseStatus[NUM_ALERT_RESPONSES] = { false };

	// Check each alert status
	for (uint32_t i = 0; i < NUM_ALERTS; i++)
	{
		Alert_S* alert = alerts[i];
		if (getAlertStatus(alert) == ALERT_SET)
		{
			// Iterate through all alert responses and set them
			for (uint32_t j = 0; j < alert->numAlertResponse; j++)
			{
				const AlertResponse_E response = alert->alertResponse[j];
				// Set the alert response to active
				responseStatus[response] = true;
			}
		}
	}

	// Set BMS status based on alert
	gBms.balancingDisabled = responseStatus[DISABLE_BALANCING];
	gBms.emergencyBleed	   = responseStatus[EMERGENCY_BLEED];
	gBms.chargingDisabled  = responseStatus[DISABLE_CHARGING];
	gBms.limpModeEnabled   = responseStatus[LIMP_MODE];
	gBms.amsFaultPresent   = responseStatus[AMS_FAULT];
}

/*!
  @brief   Update BMS data statistics. Min/Max/Avg
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregatePackData(uint32_t numBmbs)
{
	Bms_S* pBms = &gBms;
	// Update BMB level stats
	aggregateBmbData(pBms->bmb, numBmbs);

	float maxBrickV	   = 0.0f;
	float minBrickV	   = 5.0f;
	float avgBrickVSum = 0.0f;

	float maxBrickTemp    = -200.0f;
	float minBrickTemp 	  = 200.0f;
	float avgBrickTempSum = 0.0f;

	float maxBoardTemp    = -200.0f;
	float minBoardTemp 	  = 200.0f;
	float avgBoardTempSum = 0.0f;

	for (int32_t i = 0; i < numBmbs; i++)
	{
		Bmb_S* pBmb = &pBms->bmb[i];

		if (pBmb->maxBrickV > maxBrickV)
		{
			maxBrickV = pBmb->maxBrickV;
		}
		if (pBmb->minBrickV < minBrickV)
		{
			minBrickV = pBmb->minBrickV;
		}

		if (pBmb->maxBrickTemp > maxBrickTemp)
		{
			maxBrickTemp = pBmb->maxBrickTemp;
		}
		if (pBmb->minBrickTemp < minBrickTemp)
		{
			minBrickTemp = pBmb->minBrickTemp;
		}

		if (pBmb->maxBoardTemp > maxBoardTemp)
		{
			maxBoardTemp = pBmb->maxBoardTemp;
		}
		if (pBmb->minBoardTemp < minBoardTemp)
		{
			minBoardTemp = pBmb->minBoardTemp;
		}

		avgBrickVSum += pBmb->avgBrickV;
		avgBrickTempSum += pBmb->avgBrickTemp;
		avgBoardTempSum += pBmb->avgBoardTemp;
	}
	pBms->maxBrickV = maxBrickV;
	pBms->minBrickV = minBrickV;
	pBms->avgBrickV = avgBrickVSum / NUM_BMBS_IN_ACCUMULATOR;
	pBms->maxBrickTemp = maxBrickTemp;
	pBms->minBrickTemp = minBrickTemp;
	pBms->avgBrickTemp = avgBrickTempSum / NUM_BMBS_IN_ACCUMULATOR;
	pBms->maxBoardTemp = maxBoardTemp;
	pBms->minBoardTemp = minBoardTemp;
	pBms->avgBoardTemp = avgBoardTempSum / NUM_BMBS_IN_ACCUMULATOR;
}

/*!
  @brief   Update the IMD status based on measured frequency and duty cycle
*/
void updateImdStatus()
{
	Bms_S* pBms = &gBms;

	pBms->imdState = getImdStatus();
}


/*!
  @brief   Update the SDC status
*/
void updateSdcStatus()
{
	Bms_S* pBms = &gBms;

	pBms->amsFaultStatus  = HAL_GPIO_ReadPin(AMS_FAULT_SDC_GPIO_Port, AMS_FAULT_SDC_Pin);
	pBms->bspdFaultStatus = HAL_GPIO_ReadPin(BSPD_FAULT_SDC_GPIO_Port, BSPD_FAULT_SDC_Pin);
	pBms->imdFaultStatus  = HAL_GPIO_ReadPin(IMD_FAULT_SDC_GPIO_Port, IMD_FAULT_SDC_Pin);
}

void updateEpaper()
{
	if((HAL_GetTick() - lastEpapUpdate) > 2000)
	{
		lastEpapUpdate = HAL_GetTick();

		Epaper_Data_S epapData;
		epapData.avgBrickV = gBms.avgBrickV;
		epapData.maxBrickV = gBms.maxBrickV;
		epapData.minBrickV = gBms.minBrickV;

		epapData.avgBrickTemp = gBms.avgBrickTemp;
		epapData.maxBrickTemp = gBms.maxBrickTemp;
		epapData.minBrickTemp = gBms.minBrickTemp;

		epapData.avgBoardTemp = gBms.avgBoardTemp;
		epapData.maxBoardTemp = gBms.maxBoardTemp;
		epapData.minBoardTemp = gBms.minBoardTemp;

		xQueueOverwrite(epaperQueueHandle, &epapData);
	}
}

void updateTractiveCurrent()
{
	static uint32_t lastCurrentUpdate = 0;
	if((HAL_GetTick() - lastCurrentUpdate) > CURRENT_SENSOR_UPDATE_PERIOD_MS)
	{
		lastCurrentUpdate = HAL_GetTick();
		getTractiveSystemCurrent(&gBms);
	}	
}

/*!
  @brief   Log non-ADC gopher can variables
*/
void updateGopherCan()
{
	if((NUM_BMBS_IN_ACCUMULATOR == 1) && (NUM_BRICKS_PER_BMB == 12))
	{
		static FLOAT_CAN_STRUCT *cellVoltageParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1Cell1Voltage_V, &seg1Cell2Voltage_V, &seg1Cell3Voltage_V, &seg1Cell4Voltage_V, &seg1Cell5Voltage_V, &seg1Cell6Voltage_V, &seg1Cell7Voltage_V, &seg1Cell8Voltage_V, &seg1Cell9Voltage_V, &seg1Cell10Voltage_V, &seg1Cell11Voltage_V, &seg1Cell12Voltage_V}
			// {&seg2Cell1Voltage_V, &seg2Cell2Voltage_V, &seg2Cell3Voltage_V, &seg2Cell4Voltage_V, &seg2Cell5Voltage_V, &seg2Cell6Voltage_V, &seg2Cell7Voltage_V, &seg2Cell8Voltage_V, &seg2Cell9Voltage_V, &seg2Cell10Voltage_V, &seg2Cell11Voltage_V, &seg2Cell12Voltage_V},
			// {&seg3Cell1Voltage_V, &seg3Cell2Voltage_V, &seg3Cell3Voltage_V, &seg3Cell4Voltage_V, &seg3Cell5Voltage_V, &seg3Cell6Voltage_V, &seg3Cell7Voltage_V, &seg3Cell8Voltage_V, &seg3Cell9Voltage_V, &seg3Cell10Voltage_V, &seg3Cell11Voltage_V, &seg3Cell12Voltage_V},
			// {&seg4Cell1Voltage_V, &seg4Cell2Voltage_V, &seg4Cell3Voltage_V, &seg4Cell4Voltage_V, &seg4Cell5Voltage_V, &seg4Cell6Voltage_V, &seg4Cell7Voltage_V, &seg4Cell8Voltage_V, &seg4Cell9Voltage_V, &seg4Cell10Voltage_V, &seg4Cell11Voltage_V, &seg4Cell12Voltage_V},
			// {&seg5Cell1Voltage_V, &seg5Cell2Voltage_V, &seg5Cell3Voltage_V, &seg5Cell4Voltage_V, &seg5Cell5Voltage_V, &seg5Cell6Voltage_V, &seg5Cell7Voltage_V, &seg5Cell8Voltage_V, &seg5Cell9Voltage_V, &seg5Cell10Voltage_V, &seg5Cell11Voltage_V, &seg5Cell12Voltage_V},
			// {&seg6Cell1Voltage_V, &seg6Cell2Voltage_V, &seg6Cell3Voltage_V, &seg6Cell4Voltage_V, &seg6Cell5Voltage_V, &seg6Cell6Voltage_V, &seg6Cell7Voltage_V, &seg6Cell8Voltage_V, &seg6Cell9Voltage_V, &seg6Cell10Voltage_V, &seg6Cell11Voltage_V, &seg6Cell12Voltage_V},
			// {&seg7Cell1Voltage_V, &seg7Cell2Voltage_V, &seg7Cell3Voltage_V, &seg7Cell4Voltage_V, &seg7Cell5Voltage_V, &seg7Cell6Voltage_V, &seg7Cell7Voltage_V, &seg7Cell8Voltage_V, &seg7Cell9Voltage_V, &seg7Cell10Voltage_V, &seg7Cell11Voltage_V, &seg7Cell12Voltage_V}
		};

		static FLOAT_CAN_STRUCT *cellTempParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1Cell1Temp_C, &seg1Cell2Temp_C, &seg1Cell3Temp_C, &seg1Cell4Temp_C, &seg1Cell5Temp_C, &seg1Cell6Temp_C, &seg1Cell7Temp_C, &seg1Cell8Temp_C, &seg1Cell9Temp_C, &seg1Cell10Temp_C, &seg1Cell11Temp_C, &seg1Cell12Temp_C}
			// {&seg2Cell1Temp_C, &seg2Cell2Temp_C, &seg2Cell3Temp_C, &seg2Cell4Temp_C, &seg2Cell5Temp_C, &seg2Cell6Temp_C, &seg2Cell7Temp_C, &seg2Cell8Temp_C, &seg2Cell9Temp_C, &seg2Cell10Temp_C, &seg2Cell11Temp_C, &seg2Cell12Temp_C},
			// {&seg3Cell1Temp_C, &seg3Cell2Temp_C, &seg3Cell3Temp_C, &seg3Cell4Temp_C, &seg3Cell5Temp_C, &seg3Cell6Temp_C, &seg3Cell7Temp_C, &seg3Cell8Temp_C, &seg3Cell9Temp_C, &seg3Cell10Temp_C, &seg3Cell11Temp_C, &seg3Cell12Temp_C},
			// {&seg4Cell1Temp_C, &seg4Cell2Temp_C, &seg4Cell3Temp_C, &seg4Cell4Temp_C, &seg4Cell5Temp_C, &seg4Cell6Temp_C, &seg4Cell7Temp_C, &seg4Cell8Temp_C, &seg4Cell9Temp_C, &seg4Cell10Temp_C, &seg4Cell11Temp_C, &seg4Cell12Temp_C},
			// {&seg5Cell1Temp_C, &seg5Cell2Temp_C, &seg5Cell3Temp_C, &seg5Cell4Temp_C, &seg5Cell5Temp_C, &seg5Cell6Temp_C, &seg5Cell7Temp_C, &seg5Cell8Temp_C, &seg5Cell9Temp_C, &seg5Cell10Temp_C, &seg5Cell11Temp_C, &seg5Cell12Temp_C},
			// {&seg6Cell1Temp_C, &seg6Cell2Temp_C, &seg6Cell3Temp_C, &seg6Cell4Temp_C, &seg6Cell5Temp_C, &seg6Cell6Temp_C, &seg6Cell7Temp_C, &seg6Cell8Temp_C, &seg6Cell9Temp_C, &seg6Cell10Temp_C, &seg6Cell11Temp_C, &seg6Cell12Temp_C},
			// {&seg7Cell1Temp_C, &seg7Cell2Temp_C, &seg7Cell3Temp_C, &seg7Cell4Temp_C, &seg7Cell5Temp_C, &seg7Cell6Temp_C, &seg7Cell7Temp_C, &seg7Cell8Temp_C, &seg7Cell9Temp_C, &seg7Cell10Temp_C, &seg7Cell11Temp_C, &seg7Cell12Temp_C}
		};

		static FLOAT_CAN_STRUCT *boardTempParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1BMBBoardTemp1_C, &seg1BMBBoardTemp2_C, &seg1BMBBoardTemp3_C, &seg1BMBBoardTemp4_C}
			// {&seg2BMBBoardTemp1_C, &seg2BMBBoardTemp2_C, &seg2BMBBoardTemp3_C, &seg2BMBBoardTemp4_C},
			// {&seg3BMBBoardTemp1_C, &seg3BMBBoardTemp2_C, &seg3BMBBoardTemp3_C, &seg3BMBBoardTemp4_C},
			// {&seg4BMBBoardTemp1_C, &seg4BMBBoardTemp2_C, &seg4BMBBoardTemp3_C, &seg4BMBBoardTemp4_C},
			// {&seg5BMBBoardTemp1_C, &seg5BMBBoardTemp2_C, &seg5BMBBoardTemp3_C, &seg5BMBBoardTemp4_C},
			// {&seg6BMBBoardTemp1_C, &seg6BMBBoardTemp2_C, &seg6BMBBoardTemp3_C, &seg6BMBBoardTemp4_C},
			// {&seg7BMBBoardTemp1_C, &seg7BMBBoardTemp2_C, &seg7BMBBoardTemp3_C, &seg7BMBBoardTemp4_C}
		};

		static FLOAT_CAN_STRUCT *cellVoltageStatsParams[NUM_BMBS_IN_ACCUMULATOR][4] =
		{
			{&seg1Voltage_V, &seg1AveCellVoltage_V, &seg1MaxCellVoltage_V, &seg1MinCellVoltage_V}
			// {&seg2Voltage_V, &seg2AveCellVoltage_V, &seg2MaxCellVoltage_V, &seg2MinCellVoltage_V},
			// {&seg3Voltage_V, &seg3AveCellVoltage_V, &seg3MaxCellVoltage_V, &seg3MinCellVoltage_V},
			// {&seg4Voltage_V, &seg4AveCellVoltage_V, &seg4MaxCellVoltage_V, &seg4MinCellVoltage_V},
			// {&seg5Voltage_V, &seg5AveCellVoltage_V, &seg5MaxCellVoltage_V, &seg5MinCellVoltage_V},
			// {&seg6Voltage_V, &seg6AveCellVoltage_V, &seg6MaxCellVoltage_V, &seg6MinCellVoltage_V},
			// {&seg7Voltage_V, &seg7AveCellVoltage_V, &seg7MaxCellVoltage_V, &seg7MinCellVoltage_V}
		};

		// static FLOAT_CAN_STRUCT *cellTempStatsParams[NUM_BMBS_IN_ACCUMULATOR][3] =
		// {
		// 	{&seg1AveCellTemp_C, &seg1MaxCellTemp_C, &seg1MinCellTemp_C}
		// 	{&seg2AveCellTemp_C, &seg2MaxCellTemp_C, &seg2MinCellTemp_C},
		// 	{&seg3AveCellTemp_C, &seg3MaxCellTemp_C, &seg3MinCellTemp_C},
		// 	{&seg4AveCellTemp_C, &seg4MaxCellTemp_C, &seg4MinCellTemp_C},
		// 	{&seg5AveCellTemp_C, &seg5MaxCellTemp_C, &seg5MinCellTemp_C},
		// 	{&seg6AveCellTemp_C, &seg6MaxCellTemp_C, &seg6MinCellTemp_C},
		// 	{&seg7AveCellTemp_C, &seg7MaxCellTemp_C, &seg7MinCellTemp_C}
		// };

		// static FLOAT_CAN_STRUCT *boardTempStatsParams[NUM_BMBS_IN_ACCUMULATOR][3] =
		// {
		// 	{&seg1BMBAveBoardTemp_C, &seg1BMBMaxBoardTemp_C, &seg1BMBMinBoardTemp_C}
		// 	{&seg2BMBAveBoardTemp_C, &seg2BMBMaxBoardTemp_C, &seg2BMBMinBoardTemp_C},
		// 	{&seg3BMBAveBoardTemp_C, &seg3BMBMaxBoardTemp_C, &seg3BMBMinBoardTemp_C},
		// 	{&seg4BMBAveBoardTemp_C, &seg4BMBMaxBoardTemp_C, &seg4BMBMinBoardTemp_C},
		// 	{&seg5BMBAveBoardTemp_C, &seg5BMBMaxBoardTemp_C, &seg5BMBMinBoardTemp_C},
		// 	{&seg6BMBAveBoardTemp_C, &seg6BMBMaxBoardTemp_C, &seg6BMBMinBoardTemp_C},
		// 	{&seg7BMBAveBoardTemp_C, &seg7BMBMaxBoardTemp_C, &seg7BMBMinBoardTemp_C}
		// };

		// static U8_CAN_STRUCT *balswenParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		// {
		// 	{&seg1Cell1BalanceEnable_state, &seg1Cell2BalanceEnable_state, &seg1Cell3BalanceEnable_state, &seg1Cell4BalanceEnable_state, &seg1Cell5BalanceEnable_state, &seg1Cell6BalanceEnable_state, &seg1Cell7BalanceEnable_state, &seg1Cell8BalanceEnable_state, &seg1Cell9BalanceEnable_state, &seg1Cell10BalanceEnable_state, &seg1Cell11BalanceEnable_state, &seg1Cell12BalanceEnable_state}
		// 	{&seg2Cell1BalanceEnable_state, &seg2Cell2BalanceEnable_state, &seg2Cell3BalanceEnable_state, &seg2Cell4BalanceEnable_state, &seg2Cell5BalanceEnable_state, &seg2Cell6BalanceEnable_state, &seg2Cell7BalanceEnable_state, &seg2Cell8BalanceEnable_state, &seg2Cell9BalanceEnable_state, &seg2Cell10BalanceEnable_state, &seg2Cell11BalanceEnable_state, &seg2Cell12BalanceEnable_state},
		// 	{&seg3Cell1BalanceEnable_state, &seg3Cell2BalanceEnable_state, &seg3Cell3BalanceEnable_state, &seg3Cell4BalanceEnable_state, &seg3Cell5BalanceEnable_state, &seg3Cell6BalanceEnable_state, &seg3Cell7BalanceEnable_state, &seg3Cell8BalanceEnable_state, &seg3Cell9BalanceEnable_state, &seg3Cell10BalanceEnable_state, &seg3Cell11BalanceEnable_state, &seg3Cell12BalanceEnable_state},
		// 	{&seg4Cell1BalanceEnable_state, &seg4Cell2BalanceEnable_state, &seg4Cell3BalanceEnable_state, &seg4Cell4BalanceEnable_state, &seg4Cell5BalanceEnable_state, &seg4Cell6BalanceEnable_state, &seg4Cell7BalanceEnable_state, &seg4Cell8BalanceEnable_state, &seg4Cell9BalanceEnable_state, &seg4Cell10BalanceEnable_state, &seg4Cell11BalanceEnable_state, &seg4Cell12BalanceEnable_state},
		// 	{&seg5Cell1BalanceEnable_state, &seg5Cell2BalanceEnable_state, &seg5Cell3BalanceEnable_state, &seg5Cell4BalanceEnable_state, &seg5Cell5BalanceEnable_state, &seg5Cell6BalanceEnable_state, &seg5Cell7BalanceEnable_state, &seg5Cell8BalanceEnable_state, &seg5Cell9BalanceEnable_state, &seg5Cell10BalanceEnable_state, &seg5Cell11BalanceEnable_state, &seg5Cell12BalanceEnable_state},
		// 	{&seg6Cell1BalanceEnable_state, &seg6Cell2BalanceEnable_state, &seg6Cell3BalanceEnable_state, &seg6Cell4BalanceEnable_state, &seg6Cell5BalanceEnable_state, &seg6Cell6BalanceEnable_state, &seg6Cell7BalanceEnable_state, &seg6Cell8BalanceEnable_state, &seg6Cell9BalanceEnable_state, &seg6Cell10BalanceEnable_state, &seg6Cell11BalanceEnable_state, &seg6Cell12BalanceEnable_state},
		// 	{&seg7Cell1BalanceEnable_state, &seg7Cell2BalanceEnable_state, &seg7Cell3BalanceEnable_state, &seg7Cell4BalanceEnable_state, &seg7Cell5BalanceEnable_state, &seg7Cell6BalanceEnable_state, &seg7Cell7BalanceEnable_state, &seg7Cell8BalanceEnable_state, &seg7Cell9BalanceEnable_state, &seg7Cell10BalanceEnable_state, &seg7Cell11BalanceEnable_state, &seg7Cell12BalanceEnable_state}
		// };

		// Log gcan variables across the alloted time period in data chunks
		static uint32_t lastGcanUpdate = 0;
		if((HAL_GetTick() - lastGcanUpdate) > GOPHER_CAN_LOGGING_PERIOD_MS)
		{
			lastGcanUpdate = HAL_GetTick();

			// Log the accumulator data according to the current GCAN logging state
			static uint8_t gcanUpdateState = 0;
			switch (gcanUpdateState)
			{
				case GCAN_SEGMENT_1:
				// case GCAN_SEGMENT_2:
				// case GCAN_SEGMENT_3:
				// case GCAN_SEGMENT_4:
				// case GCAN_SEGMENT_5:
				// case GCAN_SEGMENT_6:
				// case GCAN_SEGMENT_7:
					
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][0], gBms.bmb[gcanUpdateState].segmentV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][1], gBms.bmb[gcanUpdateState].avgBrickV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][2], gBms.bmb[gcanUpdateState].maxBrickV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][3], gBms.bmb[gcanUpdateState].minBrickV);
					

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_float(cellVoltageParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].brickV[i]);
					}

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_float(cellTempParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].brickTemp[i]);
					}

					for (int32_t i = 0; i < NUM_BOARD_TEMP_PER_BMB; i++)
					{
						update_and_queue_param_float(boardTempParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].boardTemp[i]);
					}
					break;
				
				// case GCAN_CELL_TEMP_STATS:
				// 	for (int32_t i = 0; i < NUM_BMBS_IN_ACCUMULATOR; i++)
				// 	{
				// 		update_and_queue_param_float(cellTempStatsParams[i][0], gBms.bmb[i].avgBrickTemp);
				// 		update_and_queue_param_float(cellTempStatsParams[i][1], gBms.bmb[i].maxBrickTemp);
				// 		update_and_queue_param_float(cellTempStatsParams[i][2], gBms.bmb[i].minBrickTemp);
				// 	}

				// 	for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
				// 	{
				// 		update_and_queue_param_u8(balswenParams[0][i], gBms.bmb[0].balSwEnabled[i]);
				// 	}

				// 	// TODO Populate fault data
				// 	update_and_queue_param_u8(&amsFault_state, 0);
				// 	update_and_queue_param_u8(&bspdFault_state, 0);

				// 	break;
				// case GCAN_BOARD_TEMP_STATS:
				// 	for (int32_t i = 0; i < NUM_BMBS_IN_ACCUMULATOR; i++)
				// 	{
				// 		update_and_queue_param_float(boardTempStatsParams[i][0], gBms.bmb[i].avgBoardTemp);
				// 		update_and_queue_param_float(boardTempStatsParams[i][1], gBms.bmb[i].maxBoardTemp);
				// 		update_and_queue_param_float(boardTempStatsParams[i][2], gBms.bmb[i].minBoardTemp);
				// 	}

				// 	for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
				// 	{
				// 		update_and_queue_param_u8(balswenParams[1][i], gBms.bmb[1].balSwEnabled[i]);
				// 	}

				// 	// TODO Populate fault data
				// 	update_and_queue_param_u8(&imdFault_state, 0);
				// 	update_and_queue_param_u8(&imdFaultInfo_state, 0);
					
				// 	break;
				// case GCAN_BALSWEN:
				// 	for (int32_t i = 2; i < NUM_BMBS_IN_ACCUMULATOR; i++)
				// 	{
				// 		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
				// 		{
				// 			update_and_queue_param_u8(balswenParams[i][j], gBms.bmb[i].balSwEnabled[j]);
				// 		}
				// 	}

				// 	//TODO Add sensor status bytes
				// 	break;

				default:
					gcanUpdateState = 0;
					break;
			}

			// Cycle Gcan state and wrap to 0 if needed
			gcanUpdateState = (gcanUpdateState++) % NUM_GCAN_STATES;

			service_can_tx(&hcan2);
		}
	}
}
