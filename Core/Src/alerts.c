/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "alerts.h"
#include "timer.h"

typedef struct
{
    AlertStatus_E alertStatus;
    Timer_S alertTimer;
    const uint32_t setTime_MS;
    const uint32_t clearTime_MS;
} Alert_S;


AlertStatus_E overvoltageAlertStatus(Bms_S *bms)
{
    return ALERT_CLEARED;
}

void runAlertMonitor(Bms_S *bms, Alert_S alert)
{
    if (alert.alertStatus == ALERT_CLEARED)
    {
        // Determine if we need to set the alert

    }

}