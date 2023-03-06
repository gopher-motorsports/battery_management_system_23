#ifndef INC_ALERTS_H_
#define INC_ALERTS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "bms.h"
#include "timer.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define OVERVOLTAGE_ALERT_SET_TIME_MS       2000
#define OVERVOLTAGE_ALERT_CLEAR_TIME_MS     2000

#define UNDERVOLTAGE_ALERT_SET_TIME_MS      2000
#define UNDERVOLTAGE_ALERT_CLEAR_TIME_MS    2000

#define CELL_IMBALANCE_ALERT_SET_TIME_MS    1000
#define CELL_IMBALANCE_ALERT_CLEAR_TIME_MS  1000

#define OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS   500
#define OVERTEMPERATURE_WARNING_ALERT_CLEAR_TIME_MS 500

#define OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS     2000
#define OVERTEMPERATURE_FAULT_ALERT_CLEAR_TIME_MS   2000


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

typedef bool (*AlertConditionPresent)(Bms_S*);
typedef struct
{
    AlertStatus_E alertStatus;
    Timer_S alertTimer;
    const uint32_t setTime_MS;
    const uint32_t clearTime_MS;
    const AlertConditionPresent alertConditionPresent;
} Alert_S;


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
// Alert Data
extern Alert_S overvoltageAlert;
extern Alert_S undervoltageAlert;
extern Alert_S cellImbalanceAlert;
extern Alert_S overtemperatureWarningAlert;
extern Alert_S overtemperatureFaultAlert;


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
