/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "mainTask.h"
#include "bms.h"
#include "debug.h"

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void runMain()
{
	static Bms_State_E bmsState = BMS_STATE_INIT;
	switch (bmsState)
	{
		case BMS_STATE_INIT: 
			bmsState = runBmsInit();
			break;

		case BMS_STATE_NOMINAL: 
			bmsState = runBmsNominal();
			break;

		case BMS_STATE_CHARGING: 
			bmsState = runBmsCharging();
			break;

		case BMS_STATE_FAULT: 
			bmsState = runBmsFault();
			break;

		default:
			bmsState = BMS_STATE_INIT;
	}
	DebugData();
}
