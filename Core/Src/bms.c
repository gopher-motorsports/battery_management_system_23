#include "main.h"
#include "bms.h"
#include "bmb.h"
#include "debug.h"

static uint32_t lastBmbUpdate = 0;
static uint32_t lastBmbDiagnostic = 0;
static bool diagnosticRequested = false;
static bool diagnosticInProgress = false;
static Bmb_Fault_State_E diagnosticState = BMB_NO_FAULT;


#define INIT_BMS_BMB_ARRAY \
{ \
    [0 ... NUM_BMBS_PER_PACK-1] = {.bmbIdx = __COUNTER__} \
}

Bms_S gBms = 
{
    .numBmbs = NUM_BMBS_PER_PACK,
    .bmb = INIT_BMS_BMB_ARRAY
};
static void disableBmbBalancing(Bmb_S* bmb);

static void disableBmbBalancing(Bmb_S* bmb)
{
	for (int i = 0; i < NUM_BRICKS_PER_BMB; i++)
	{
		bmb->balSwRequested[i] = false;
	}
}

/*!
  @brief   Initialization function for the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void initBatteryPack(uint32_t numBmbs)
{
	Bms_S* pBms = &gBms;

	pBms->numBmbs = NUM_BMBS_PER_PACK;

	memset (pBms, 0, sizeof(Bms_S));

	initBmbs(numBmbs);

	pBms->numBmbs = numBmbs;

	for(uint32_t i = 0; i < numBmbs; i++)
	{
		pBms->bmb[i].fault = BMB_NO_FAULT;
	}
}

void updatePackData(uint32_t numBmbs)
{
	if(diagnosticInProgress)
	{
		Scan_Status_E scanStatus = scanComplete(numBmbs);

		if(scanStatus == SCAN_ROUTINE_COMPLETE)
		{
			Debug("BMB Diagnostic failed to complete");
			diagnosticInProgress = false;
		}
		else if(scanStatus == SCAN_DIAGNOSTIC_COMPLETE)
		{
			Bms_S* pBms = &gBms;
			updateDiagnosticData(pBms->bmb, numBmbs);
			cycleDiagnosticMode();
			if(!performDiagnostic(numBmbs))
			{
				diagnosticInProgress = false;
				performAcquisition(numBmbs);
			}
		}
		else{
			return;
		}
	}

	if(HAL_GetTick() - lastBmbDiagnostic >= 10000)
	{
		lastBmbDiagnostic = HAL_GetTick();
		diagnosticRequested = true;
		
	}

	if(HAL_GetTick() - lastBmbUpdate >= 50)
	{
		lastBmbUpdate = HAL_GetTick();

		Scan_Status_E scanStatus = scanComplete(numBmbs);

		if(scanStatus == SCAN_ROUTINE_COMPLETE)
		{
			Bms_S* pBms = &gBms;
			updateCellData(pBms->bmb, numBmbs);
			updateTempData(pBms->bmb, numBmbs);
			aggregateBmbData(pBms->bmb, numBmbs);
		}
		else if(scanStatus == SCAN_INCOMPLETE)
		{
			lastBmbUpdate = HAL_GetTick() - 50;
			return;
		}
		
		if(diagnosticRequested)
		{
			cycleDiagnosticMode();
			performDiagnostic(numBmbs);
			diagnosticRequested = false;
			diagnosticInProgress = true;
		}
		else
		{
			performAcquisition(numBmbs);
		}
	}	
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
		for (int i = 0; i < numBmbs; i++)
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
		for (int i = 0; i < numBmbs; i++)
		{
			// Iterate through all bricks and determine whether they should be bled or not
			for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
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
		for (int i = 0; i < numBmbs; i++)
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
	for (int i = 0; i < numBmbs; i++)
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

		avgBrickVSum += pBmb->avgBrickV;
		avgBrickTempSum += pBmb->avgBrickTemp;
	}
	pBms->maxBrickV = maxBrickV;
	pBms->minBrickV = minBrickV;
	pBms->avgBrickV = avgBrickVSum / NUM_BMBS_PER_PACK;
	pBms->maxBrickTemp = maxBrickTemp;
	pBms->minBrickTemp = minBrickTemp;
	pBms->avgBrickTemp = avgBrickTempSum / NUM_BMBS_PER_PACK;
}

void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage)
{
	Bms_S* pBms = &gBms;

	// Clamp target brick voltage if too low
	if (targetBrickVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		targetBrickVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}


	for (int i = 0; i < numBmbs; i++)
	{
		// Iterate through all bricks and determine whether they should be bled or not
		for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
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
