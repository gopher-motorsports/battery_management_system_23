#include "bms.h"
#include "bmb.h"


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
}

void updatePackData(uint32_t numBmbs)
{
	// TODO - call underlying bmb.c functions
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
