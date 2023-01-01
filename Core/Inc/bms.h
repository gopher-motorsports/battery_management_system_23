#ifndef INC_BMS_H_
#define INC_BMS_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <bmb.h>


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
// The number of BMBs per battery pack
#define NUM_BMBS_PER_PACK		1
// Max allowable voltage difference between bricks for balancing
#define BALANCE_THRESHOLD_V		0.002f


/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */
typedef struct
{
	uint32_t numBmbs;
	Bmb_S bmb[NUM_BMBS_PER_PACK];
} Bms_S;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */
void initBatteryPack(uint32_t numBmbs);

void balancePack(uint32_t numBmbs, bool balanceRequested);

#endif /* INC_BMS_H_ */
