#ifndef INC_ALERTS_H_
#define INC_ALERTS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "bms.h"
#include "shared.h"
#include "timer.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS     1000
#define OVERVOLTAGE_WARNING_ALERT_CLEAR_TIME_MS   1000

#define OVERVOLTAGE_FAULT_ALERT_SET_TIME_MS       2000
#define OVERVOLTAGE_FAULT_ALERT_CLEAR_TIME_MS     2000

#define UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS    2000
#define UNDERVOLTAGE_WARNING_ALERT_CLEAR_TIME_MS  2000

#define UNDERVOLTAGE_FAULT_ALERT_SET_TIME_MS      2000
#define UNDERVOLTAGE_FAULT_ALERT_CLEAR_TIME_MS    2000

#define MAX_CELL_IMBALANCE_V                      0.2f
#define CELL_IMBALANCE_ALERT_SET_TIME_MS          1000
#define CELL_IMBALANCE_ALERT_CLEAR_TIME_MS        1000

#define OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS   500
#define OVERTEMPERATURE_WARNING_ALERT_CLEAR_TIME_MS 500

#define OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS     2000
#define OVERTEMPERATURE_FAULT_ALERT_CLEAR_TIME_MS   2000

#define SDC_FAULT_ALERT_SET_TIME_MS   0
#define SDC_FAULT_ALERT_CLEAR_TIME_MS 0

#define CURRENT_SENSOR_ERROR_ALERT_SET_TIME_MS        2000
#define CURRENT_SENSOR_ERROR_ALERT_CLEAR_TIME_MS      2000

#define BMB_COMMUNICATION_FAILURE_ALERT_SET_TIME_MS   2000
#define BMB_COMMUNICATION_FAILURE_ALERT_CLEAR_TIME_MS 2000

#define BAD_VOLTAGE_SENSE_STATUS_ALERT_SET_TIME_MS    2000
#define BAD_VOLTAGE_SENSE_STATUS_ALERT_CLEAR_TIME_MS  2000

#define BAD_BRICK_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS    2000
#define BAD_BRICK_TEMP_SENSE_STATUS_ALERT_CLEAR_TIME_MS  2000

#define BAD_BOARD_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS    2000
#define BAD_BOARD_TEMP_SENSE_STATUS_ALERT_CLEAR_TIME_MS  2000

// The minimum percent of brick temps that must be monitored to pass rules
#define MIN_PERCENT_BRICK_TEMPS_MONITORED             25
#define INSUFFICIENT_TEMP_SENSOR_ALERT_SET_TIME_MS    2000
#define INSUFFICIENT_TEMP_SENSOR_ALERT_CLEAR_TIME_MS  2000

#define STACK_VS_SEGMENT_IMBALANCE_ALERT_SET_TIME_MS    1000
#define STACK_VS_SEGMENT_IMBALANCE_ALERT_CLEAR_TIME_MS  1000

#define CHARGER_OVERVOLTAGE_ALERT_SET_TIME_MS      2000
#define CHARGER_OVERVOLTAGE_ALERT_CLEAR_TIME_MS    2000

#define CHARGER_OVERCURRENT_ALERT_SET_TIME_MS      2000
#define CHARGER_OVERCURRENT_ALERT_CLEAR_TIME_MS    2000

#define CHARGER_VOLTAGE_MISMATCH_ALERT_SET_TIME_MS      2000
#define CHARGER_VOLTAGE_MISMATCH_ALERT_CLEAR_TIME_MS    2000

#define CHARGER_CURRENT_MISMATCH_ALERT_SET_TIME_MS      2000
#define CHARGER_CURRENT_MISMATCH_ALERT_CLEAR_TIME_MS    2000

#define CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS      2000
#define CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS    2000

/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum
{
    ALERT_CLEARED = 0,	// Indicates alert has not been set yet
    ALERT_LATCHED,		// Indicates alert is no longer present, but was at one point (only applies for latching alerts)
    ALERT_SET			// Indicates alert is currently set
} AlertStatus_E;


/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

// Forward declaration of Bms_S struct
typedef struct Bms Bms_S;

typedef bool (*AlertConditionPresent)(Bms_S*);
typedef struct
{
    const char* alertName;
    // Whether the alert is latching or not
    const bool latching;
    // The current status of the alert
    AlertStatus_E alertStatus;
    // The timer used for qualifying the alert set/clear condition
    Timer_S alertTimer;
    // The time in ms required for the alert to be set
    const uint32_t setTime_MS;
    // The time in ms required for the alert to clear
    const uint32_t clearTime_MS;
    // Function pointer used to determine whether the alert is present or not
    const AlertConditionPresent alertConditionPresent;
    // The number of alert responses for this alert
    const uint32_t numAlertResponse;
    // Array of alert responses
    const AlertResponse_E* alertResponse;
} Alert_S;


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
// The total number of alerts
extern const uint32_t NUM_ALERTS; 
// Array of all alert structs
extern Alert_S* alerts[];


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Get the status of any given alert
  @param   alert - The alert data structure whose status to read
  @return  The current status of the alert
*/
AlertStatus_E getAlertStatus(Alert_S* alert);

/*!
  @brief   Run the alert monitor to update the status of the alert
  @param   bms - The BMS data structure
  @param   alert - The Alert data structure
*/
void runAlertMonitor(Bms_S* bms, Alert_S* alert);

#endif /* INC_ALERTS_H_ */
