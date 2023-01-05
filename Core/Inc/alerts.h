#ifndef INC_ALERTS_H_
#define INC_ALERTS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "bms.h"
#include "timer.h"


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


typedef enum
{
    ALERT_CLEARED = 0,
    ALERT_SET
} AlertStatus_E;

typedef bool (*AlertConditionPresent)(Bms_S*);
typedef struct
{
    AlertStatus_E alertStatus;
    Timer_S alertTimer;
    const uint32_t setTime_MS;
    const uint32_t clearTime_MS;
    const AlertConditionPresent alertConditionPresent;
} Alert_S;

// Alert Data
extern Alert_S overvoltageAlert;
extern Alert_S undervoltageAlert;
extern Alert_S cellImbalanceAlert;
extern Alert_S overtemperatureWarningAlert;
extern Alert_S overtemperatureFaultAlert;


AlertStatus_E alertStatus(Alert_S* alert);

void runAlertMonitor(Bms_S* bms, Alert_S* alert);

#endif /* INC_ALERTS_H_ */
