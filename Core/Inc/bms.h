#ifndef INC_BMS_H_
#define INC_BMS_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "alerts.h"
#include "shared.h"
#include "main.h"
#include "bmb.h"
#include "imd.h"
#include "soc.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The number of BMBs in the accumulator
#define NUM_BMBS_IN_ACCUMULATOR				7 

// Max allowable voltage difference between bricks for balancing
#define BALANCE_THRESHOLD_V					0.001f

// The maximum cell temperature where charging is allowed
#define MAX_CELL_TEMP_CHARGING_ALLOWED_C	50.0f

// The delay between consecutive current sensor updates
#define CURRENT_SENSOR_UPDATE_PERIOD_MS 	4

// The rate at which we update the coulomb counter
#define SOC_AND_SOE_UPDATE_PERIOD_MS		10

// The delay between consecutive bmb updates
#define VOLTAGE_DATA_UPDATE_PERIOD_MS		50

// Gophercan variable logging frequency. This value will be divided by the number of transactions
// Frequency cannot exceed HW CONFIG max logging frequency
#define GOPHER_CAN_LOGGING_FREQUENCY_HZ		1

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
	GCAN_SEGMENT_1 = 0,
	// GCAN_SEGMENT_2,
	// GCAN_SEGMENT_3,
	// GCAN_SEGMENT_4,
	// GCAN_SEGMENT_5,
	// GCAN_SEGMENT_6,
	// GCAN_SEGMENT_7,
	// GCAN_CELL_TEMP_STATS,
	// GCAN_BOARD_TEMP_STATS,
	// GCAN_BALSWEN,
	NUM_GCAN_STATES
} Gcan_State_E;

// The delay between consecutive additions to gcan logging
#define GOPHER_CAN_LOGGING_PERIOD_MS	(1000 / (GOPHER_CAN_LOGGING_FREQUENCY_HZ * NUM_GCAN_STATES))

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct Bms
{
	uint32_t numBmbs;
	Bmb_S bmb[NUM_BMBS_IN_ACCUMULATOR];

	float accumulatorVoltage;

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
	Sensor_Status_E tractiveSystemCurrentStatus;
	float tractiveSystemCurrent;

	Soc_S soc;

	bool balancingDisabled;
	bool emergencyBleed;
	bool chargingDisabled;
	bool limpModeEnabled;
	bool amsFaultPresent;

	bool bspdFaultStatus;
	bool imdFaultStatus;
	bool amsFaultStatus;

	Bms_Hardware_State_E bmsHwState;

	bool chargerConnected;
	Charger_Data_S chargerData;
} Bms_S;


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
  @brief   Updates all BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain\
*/
void updatePackData(uint32_t numBmbs);

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

/*!
  @brief   Balance the battery pack to a specified target brick voltage.
  @param   numBmbs - The number of Battery Management Boards (BMBs) in the pack.
  @param   targetBrickVoltage - The target voltage for each brick in the pack.
*/
void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage);


/*!
  @brief   Check and handle alerts for the BMS by running alert monitors, accumulating alert statuses,
           and setting BMS status based on the alerts.
*/
void checkAndHandleAlerts();

/*!
  @brief   Update the IMD status based on measured frequency and duty cycle
*/
void updateImdStatus();

/*!
  @brief   Update the epaper display with current data
*/
void updateEpaper();

/*!
  @brief   Update the tractive system current
*/
void updateTractiveCurrent();

void updateStateOfChargeAndEnergy();

/*!
  @brief   Log non-ADC gopher can variables
*/
void updateGopherCan();

/*!
  @brief   Check for new charger messages
*/
void checkForNewChargerInfo();

/*!
  @brief   Perform accumulator charge sequence
*/
void chargeAccumulator();

#endif /* INC_BMS_H_ */
