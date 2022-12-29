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


