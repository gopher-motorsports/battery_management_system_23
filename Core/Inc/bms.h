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

// The number of BMBs in the accumulator
#define NUM_BMBS_IN_ACCUMULATOR		1
// Max allowable voltage difference between bricks for balancing
#define BALANCE_THRESHOLD_V			0.002f
// The maximum cell temperature where charging is allowed
#define MAX_CELL_TEMP_CHARGING_ALLOWED_C	50.0f


// The delay between consecutive attempted internal resistance calcs in millis
#define INTERNAL_RESISTANCE_UPDATE_PERIOD_MS 	100
// The amount of time voltage and current data can be considered for an internal resistance calc;
#define INTERNAL_RESISTANCE_TIME_BUFFER_MS 		10000
// The number of elements in the data buffer array
#define INTERNAL_RESISTANCE_BUFFER_SIZE			INTERNAL_RESISTANCE_TIME_BUFFER_MS / INTERNAL_RESISTANCE_UPDATE_PERIOD_MS

// The minimum difference in current data points to calculate internal resistance in Amps
#define INTERNAL_RESISTANCE_MIN_CURRENT_DELTA	20

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
	SENSE_SNA = 0,	// Value on startup
	SENSE_GOOD,		// Data nominal
	SENSE_MIA			// Data wasn't aquired
} Sensor_Status_E;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct
{
	uint32_t numBmbs;
	Bmb_S bmb[NUM_BMBS_IN_ACCUMULATOR];

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

	float packResistance;

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
  @returns bool True if initialization successful, false otherwise
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

/*!
  @brief   Update the tractive system current
*/
void updateTractiveCurrent();

/*!
  @brief   Calculate the internal resistance of the battery pack
*/
void updateInternalResistance();

#endif /* INC_BMS_H_ */
