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
#define CHARGER_CAN_ID_TX                      0x1806E5F4
#define CHARGER_CAN_ID_RX                      0x18FF50E5

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
#define MAX_CHARGE_CURRENT                  21.0f

// Charger output validation thresholds 
// The difference between the charger output and accumulator data must fall below these thresholds
#define CHARGER_VOLTAGE_THRESHOLD           15.0f
#define CHARGER_CURRENT_THRESHOLD           5.0f

/* ==================================================================== */
/* ========================= ENUMERATED TYPES ========================= */
/* ==================================================================== */

typedef enum
{
    CHARGER_HARDWARE_FAULT,
    CHARGER_OVERTEMP_FAULT,
    CHARGER_INPUT_VOLTAGE_FAULT,
    CHRAGER_REVERSE_POLARITY_FAULT,
    CHARGER_COMMUNICATION_FAULT,
    NUM_CHARGER_FAULTS
} Charger_Error_E;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct
{
  float chargerVoltage;
	float chargerCurrent;
	bool chargerStatus[NUM_CHARGER_FAULTS];

} Charger_Data_S;

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
