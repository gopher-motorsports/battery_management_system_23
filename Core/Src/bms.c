#include <bms.h>
#include "bmb.h"

Bms_S gBms;

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

void tempBalance(uint32_t numBmbs, bool balance)
{
	Bms_S* pBms = &gBms;

	if (balance)
	{
		float targetVoltage = 5.0f;

		for (int i = 0; i < numBmbs; i++)
		{
			if (pBms->bmb[i].minBrickV + BALANCE_THRESHOLD_V < targetVoltage)
			{
				targetVoltage = pBms->bmb[i].minBrickV + BALANCE_THRESHOLD_V;
			}
		}

		for (int i = 0; i < numBmbs; i++)
		{
			for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
			{
				if (pBms->bmb[i].brickV[j] > targetVoltage)
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


