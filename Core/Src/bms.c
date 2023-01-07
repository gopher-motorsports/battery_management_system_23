#include <bms.h>
#include "bmb.h"

Bms_S gBms;

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
			// Verify that board or cells not too hot to balance
			if ((pBms->bmb[i].maxBoardTemp > MAX_BOARD_TEMP_BALANCING_ALLOWED_C) ||
				(pBms->bmb[i].maxBrickTemp > MAX_CELL_TEMP_BLEEDING_ALLOWED_C))
			{
				disableBmbBalancing(&pBms->bmb[i]);
				continue;
			}

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
