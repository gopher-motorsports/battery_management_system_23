/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "alerts.h"


AlertStatus_E alertStatus(Alert_S* alert)
{
    return alert->alertStatus;
}

void runAlertMonitor(Bms_S* bms, Alert_S* alert)
{
    if (alert->alertStatus == ALERT_CLEARED)
    {
        // Determine if we need to set the alert
        if (alert->alertConditionPresent(bms))
        {
            // Increment alert timer by 10ms
            updateTimer_10ms(&alert->alertTimer);
        }
        else
        {
            // Reset alert timer
            clearTimer(&alert->alertTimer);
        }

        // Determine if alert was set
        if (checkTimerExpired(&alert->alertTimer))
        {
            // Timer expired - Set alert
            alert->alertStatus = ALERT_SET;
            // Load timer with alert clear time
            configureTimer(&alert->alertTimer, alert->clearTime_MS);
        }

    }
    else if (alert->alertStatus == ALERT_SET)
    {
        // Determine if we can clear the alert
        if (!alert->alertConditionPresent(bms))
        {
            // Increment clear timer by 10ms
            updateTimer_10ms(&alert->alertTimer);
        }
        else
        {
            // Alert conditions detected. Reset clear timer
            clearTimer(&alert->alertTimer);
        }

        // Determine if alert was cleared
        if (checkTimerExpired(&alert->alertTimer))
        {
            // Timer expired - Clear alert
            alert->alertStatus = ALERT_CLEARED;
            // Load timer with alert set time
            configureTimer(&alert->alertTimer, alert->setTime_MS);
        }
    }
}

bool overvoltagePresent(Bms_S *bms)
{
    return (bms->maxBrickV > MAX_BRICK_VOLTAGE);
}

bool undervoltagePresent(Bms_S *bms)
{
    return (bms->minBrickV < MIN_BRICK_VOLTAGE);
}

Alert_S overvoltageAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERVOLTAGE_ALERT_SET_TIME_MS}, 
                            .setTime_MS = OVERVOLTAGE_ALERT_SET_TIME_MS, .clearTime_MS = OVERVOLTAGE_ALERT_CLEAR_TIME_MS, 
                            .alertConditionPresent = overvoltagePresent};

Alert_S undervoltageAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, UNDERVOLTAGE_ALERT_SET_TIME_MS}, 
                            .setTime_MS = UNDERVOLTAGE_ALERT_SET_TIME_MS, .clearTime_MS = UNDERVOLTAGE_ALERT_CLEAR_TIME_MS, 
                            .alertConditionPresent = undervoltagePresent};
