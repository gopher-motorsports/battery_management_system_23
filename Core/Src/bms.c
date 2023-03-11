/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "cmsis_os.h"
#include "bms.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "debug.h"
#include "GopherCAN.h"
#include "debug.h"
#include "currentSense.h"
#include "internalResistance.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define INIT_BMS_BMB_ARRAY \
{ \
    [0 ... NUM_BMBS_PER_PACK-1] = {.bmbIdx = __COUNTER__} \
}

Bms_S gBms = 
{
    .numBmbs = NUM_BMBS_PER_PACK,
    .bmb = INIT_BMS_BMB_ARRAY
};

extern osMessageQId epaperQueueHandle;

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
	Bms_S* pBms = &gBms;

	if (!initASCI())
	{
		return false;
	}

	if (!helloAll(numBmbs))
	{
		return false;
	}

	if (*numBmbs != NUM_BMBS_PER_PACK)
	{
		Debug("Number of BMBs detected (%lu) doesn't match expectation (%d)\n", *numBmbs, NUM_BMBS_PER_PACK);
		return false;
	}

	if (!initBmbs(*numBmbs))
	{
		return false;
	}

	pBms->numBmbs = *numBmbs;
	pBms->bmsHwState = BMS_NOMINAL;
	return true;
}

void updatePackData(uint32_t numBmbs)
{
	updateBmbData(gBms.bmb, numBmbs);
	putVoltageBuffer(&gBms);
}

/*!
  @brief   Handles balancing the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   balanceRequested - True if we want to balance, false otherwise
*/
void balancePack(uint32_t numBmbs, bool balanceRequested)
{
	Bms_S* pBms = &gBms;

	if (balanceRequested)
	{
		float bleedTargetVoltage = 5.0f;
		// Determine minimum voltage across entire battery pack
		for (int32_t i = 0; i < numBmbs; i++)
		{
			if (pBms->bmb[i].minBrickV + BALANCE_THRESHOLD_V < bleedTargetVoltage)
			{
				bleedTargetVoltage = pBms->bmb[i].minBrickV + BALANCE_THRESHOLD_V;
			}
		}
		// Ensure we don't overbleed the cells
		if (bleedTargetVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
		{
			bleedTargetVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
		}
		// Set bleed request on cells that have voltage higher than our bleedTargetVoltage
		for (int32_t i = 0; i < numBmbs; i++)
		{
			// Iterate through all bricks and determine whether they should be bled or not
			for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
			{
				if (pBms->bmb[i].brickV[j] > bleedTargetVoltage)
				{
					pBms->bmb[i].balSwRequested[j] = true;
				}
				else
				{
					pBms->bmb[i].balSwRequested[j] = false;
				}
			}
		}
	}
	else
	{
		// If bleeding not requested ensure balancing switches are all off
		for (int32_t i = 0; i < numBmbs; i++)
		{
			disableBmbBalancing(&pBms->bmb[i]);
		}
	}
	balanceCells(pBms->bmb, numBmbs);
}

/*!
  @brief   Update BMS data statistics. Min/Max/Avg
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregatePackData(uint32_t numBmbs)
{
	Bms_S* pBms = &gBms;
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
	pBms->avgBrickV = avgBrickVSum / NUM_BMBS_PER_PACK;
	pBms->maxBrickTemp = maxBrickTemp;
	pBms->minBrickTemp = minBrickTemp;
	pBms->avgBrickTemp = avgBrickTempSum / NUM_BMBS_PER_PACK;
	pBms->maxBoardTemp = maxBoardTemp;
	pBms->minBoardTemp = minBoardTemp;
	pBms->avgBoardTemp = avgBoardTempSum / NUM_BMBS_PER_PACK;
}

void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage)
{
	Bms_S* pBms = &gBms;

	// Clamp target brick voltage if too low
	if (targetBrickVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		targetBrickVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}


	for (int32_t i = 0; i < numBmbs; i++)
	{
		// Iterate through all bricks and determine whether they should be bled or not
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			if (pBms->bmb[i].brickV[j] > targetBrickVoltage + BALANCE_THRESHOLD_V)
			{
				pBms->bmb[i].balSwRequested[j] = true;
			}
			else
			{
				pBms->bmb[i].balSwRequested[j] = false;
			}
		}
	}
	
	balanceCells(pBms->bmb, numBmbs);
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

	pBms->amsFault  = HAL_GPIO_ReadPin(AMS_FAULT_SDC_GPIO_Port, AMS_FAULT_SDC_Pin);
	pBms->bspdFault = HAL_GPIO_ReadPin(BSPD_FAULT_SDC_GPIO_Port, BSPD_FAULT_SDC_Pin);
	pBms->imdFault  = HAL_GPIO_ReadPin(IMD_FAULT_SDC_GPIO_Port, IMD_FAULT_SDC_Pin);
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
		putCurrentBuffer(&gBms);
	}	
}
