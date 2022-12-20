/*
 * batteryPack.c
 *
 *  Created on: Dec 20, 2022
 *      Author: sebas
 */

#include "batteryPack.h"
#include "bmb.h"

extern Pack_S gPack;

void initBatteryPack(uint32_t numBmbs)
{
	Pack_S* pPack = &gPack;

	pPack->numBmbs = NUM_BMBS_PER_PACK;

	memset (pPack, 0, sizeof(Pack_S));

	initBmbs(numBmbs);

}
