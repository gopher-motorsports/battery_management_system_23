#include "bms.h"
#include "bmb.h"

Bms_S gBms;

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
		// Set bleed request on cells that have voltage higher than our bleedTargetVoltage
		for (int i = 0; i < numBmbs; i++)
		{
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
			for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
			{
				pBms->bmb[i].balSwRequested[j] = false;
			}
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
