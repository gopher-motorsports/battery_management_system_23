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

#define CELL_IMBALANCE_ALERT_SET_TIME_MS          1000
#define CELL_IMBALANCE_ALERT_CLEAR_TIME_MS        1000

#define OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS   500
#define OVERTEMPERATURE_WARNING_ALERT_CLEAR_TIME_MS 500

#define OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS     2000
#define OVERTEMPERATURE_FAULT_ALERT_CLEAR_TIME_MS   2000

#define SDC_FAULT_ALERT_SET_TIME_MS   0
#define SDC_FAULT_ALERT_CLEAR_TIME_MS 0


/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum
{
    ALERT_CLEARED = 0,
    ALERT_SET
} AlertStatus_E;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

// Forward declaration of Bms_S struct
typedef struct Bms Bms_S;

typedef bool (*AlertConditionPresent)(Bms_S*);
typedef struct
{
    AlertStatus_E alertStatus;
    Timer_S alertTimer;
    // The time in ms required for the alert to be set
    const uint32_t setTime_MS;
    // The time in ms required for the alert to clear
    const uint32_t clearTime_MS;
    const AlertConditionPresent alertConditionPresent;
    const uint32_t numAlertResponse;
    const AlertResponse_E* alertResponse;
} Alert_S;


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
// Alert Data
extern const uint32_t numAlerts; 
extern Alert_S* alerts[];


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Get the status of any given alert
  @param   alert - The alert data structure whose status to read
  @return  The current status of the alert
*/
AlertStatus_E alertStatus(Alert_S* alert);

/*!
  @brief   Run the alert monitor to update the status of the alert
  @param   bms - The BMS data structure
  @param   alert - The Alert data structure
*/
void runAlertMonitor(Bms_S* bms, Alert_S* alert);

#endif /* INC_ALERTS_H_ */
