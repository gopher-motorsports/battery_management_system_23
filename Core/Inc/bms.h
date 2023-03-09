#ifndef INC_BMS_H_
#define INC_BMS_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "main.h"
#include "bmb.h"
#include "imd.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The number of BMBs per battery pack
#define NUM_BMBS_PER_PACK			1
// Max allowable voltage difference between bricks for balancing
#define BALANCE_THRESHOLD_V			0.002f
// The maximum cell temperature where charging is allowed
#define MAX_CELL_TEMP_CHARGING_ALLOWED_C	50.0f


/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum
{
	BMS_NOMINAL = 0,
	BMS_GSNS_FAILURE,
	BMS_BMB_FAILURE
} Bms_Hardware_State_E;

typedef enum
{
	SNA = 0,	// Value on startup
	GOOD,		// Data nominal
	MIA			// Data wasn't aquired
} Sensor_Status_E;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct
{
	uint32_t numBmbs;
	Bmb_S bmb[NUM_BMBS_PER_PACK];

	float maxBrickV;
	float minBrickV;
	float avgBrickV;

	float maxBrickTemp;
	float minBrickTemp;
	float avgBrickTemp;

	float maxBoardTemp;
	float minBoardTemp;
	float avgBoardTemp;

	IMD_State_E imdState;

	Sensor_Status_E currentSensorStatusHI;
	Sensor_Status_E currentSensorStatusLO;
	float tractiveSystemCurrent;

	bool bspdFault;
	bool imdFault;
	bool amsFault;

	Bms_Hardware_State_E bmsHwState;
} Bms_S;

typedef struct
{
	float maxBrickV;
	float minBrickV;
	float avgBrickV;

	float maxBrickTemp;
	float minBrickTemp;
	float avgBrickTemp;

	float maxBoardTemp;
	float minBoardTemp;
	float avgBoardTemp;
} Epaper_Data_S;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */
/*!
  @brief   Initialization function for the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
bool initBatteryPack(uint32_t* numBmbs);

void initBmsGopherCan(CAN_HandleTypeDef* hcan);

/*!
  @brief   Handles balancing the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   balanceRequested - True if we want to balance, false otherwise
*/
void balancePack(uint32_t numBmbs, bool balanceRequested);

/*!
  @brief   Update BMS data statistics. Min/Max/Avg
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregatePackData(uint32_t numBmbs);

void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage);

/*!
  @brief   Update the IMD status based on measured frequency and duty cycle
*/
void updateImdStatus();


/*!
  @brief   Update the SDC status
*/
void updateSdcStatus();

/*!
  @brief   Update the epaper display with current data
*/
void updateEpaper();

#endif /* INC_BMS_H_ */
