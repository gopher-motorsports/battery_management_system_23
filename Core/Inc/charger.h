#ifndef CHARGER_H_
#define CHARGER_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdbool.h>
#include "shared.h"
#include "bms.h"
#include "cellData.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// Charger TX CAN EXT ID, Charger RX EXT ID is CHARGER_CAN_ID + 1
#define CHARGER_CAN_ID_TX                   0x1806E5F4
#define CHARGER_CAN_ID_RX                   0x18FF50E5

// The delay between consecutive charger request CAN messages
// The ELCON charger expects a message every 1s and will fault if a message is not recieve in 5s
#define CHARGER_UPDATE_PERIOD_MS			10 

// The timeout for consecutive charger rx data messages 
// The ELCON charger sends a message every 1s and the bms will assume the charger is not connected after this timeout
#define CHARGER_RX_TIMEOUT_MS         5000

// Hysteresis bounds for accumulator imbalance
#define MAX_CELL_IMBALANCE_THRES_HIGH       0.1f
#define MAX_CELL_IMBALANCE_THRES_LOW        0.05f

// Hysteresis bounds for max cell voltage
#define MAX_CELL_VOLTAGE_THRES_HIGH         4.21f
#define MAX_CELL_VOLTAGE_THRES_LOW          4.20f

// Charger output validation thresholds 
// The difference between the charger output and accumulator data must fall below these thresholds
#define CHARGER_VOLTAGE_MISMATCH_THRESHOLD  15.0f
#define CHARGER_CURRENT_MISMATCH_THRESHOLD  5.0f

#define HIGH_CHARGE_C_RATE                  1
#define LOW_CHARGE_C_RATE                   1.0f / 10.0f

#define LOW_CHARGE_VOLTAGE_THRES_V          3.0f

#define MIN_CHARGER_EFFICIENCY              0.9f

#define CHARGER_INPUT_POWER_W               1800.0f  

// Static_assert here to check if charge c rating is less than max c rating?

#define MAX_CHARGE_VOLTAGE_V                MAX_BRICK_VOLTAGE * NUM_BRICKS_PER_BMB * NUM_BMBS_IN_ACCUMULATOR
#define MAX_CHARGE_CURRENT_A                NUM_PARALLEL_CELLS * MAX_CHARGE_C_RATING * CELL_CAPACITY_AH
#define HIGH_CHARGE_CURRENT_A               NUM_PARALLEL_CELLS * HIGH_CHARGE_C_RATE * CELL_CAPACITY_AH
#define LOW_CHARGE_CURRENT_A                NUM_PARALLEL_CELLS * LOW_CHARGE_C_RATE * CELL_CAPACITY_AH

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Send a CAN message to the charger
  @param   voltageRequest - Charger Voltage Request
  @param   currentRequest - Charger Current Request
  @param   enable - Enable/Disable Request
*/
void sendChargerMessage(float voltageRequest, float currentRequest, bool enable);

/*!
  @brief   Update a chargerData struct with charger information
  @param   chargerData - Charger_Data_S data struct to store charger data
*/
void updateChargerData(Charger_Data_S* chargerData);

#endif /* CHARGER_H_ */
