#ifndef INC_ALERTS_H_
#define INC_ALERTS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "bms.h"


typedef enum
{
    ALERT_CLEARED = 0,
    ALERT_SET
} AlertStatus_E;


void runOvervoltageAlertMonitor(Bms_S *bms);

#endif /* INC_ALERTS_H_ */
