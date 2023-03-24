#ifndef CHARGER_H_
#define CHARGER_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "bms.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// Charger TX CAN EXT ID, Charger RX EXT ID is CHARGER_CAN_ID + 1
#define CHARGER_CAN_ID                      0x1806E5F4

// Hysteresis bounds for accumulator imbalance
#define MAX_CELL_IMBALANCE_THRES_HIGH       0.1f
#define MAX_CELL_IMBALANCE_THRES_LOW        0.05f

// Hysteresis bounds for max cell voltage
#define MAX_CELL_VOLTAGE_THRES_HIGH         4.21f
#define MAX_CELL_VOLTAGE_THRES_LOW          4.20f

// The max accumulator voltage and the requested charge voltage of the charger
#define MAX_ACCUMULATOR_VOLTAGE             MAX_CELL_VOLTAGE * NUM_BMBS_IN_ACCUMULATOR * NUM_BRICKS_PER_BMB

// The max safe charge current and the requested charge current of the charger
// VTC6 recommends 3A per 18650. 3A x 7p = 21A
#define MAX_CHRAGE_CURRENT                  21.0f

// Charger output validation thresholds 
// The difference between the charger output and accumulator data must fall below these thresholds
#define CHARGER_VOLTAGE_THRESHOLD           15.0f
#define CHARGER_CURRENT_THRESHOLD           5.0f

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Determine if the charger should be enabled or disabled, and send a request to the charger
  @param   bms - BMS data struct
*/
void updateChargerState(Bms_S* bms);

/*!
  @brief   Validate that the recieved charger CAN message indicates no faults and corresponds to the current accumulator status
  @param   bms - BMS data struct
*/
void validateChargerState(Bms_S* bms);

#endif /* CHARGER_H_ */
