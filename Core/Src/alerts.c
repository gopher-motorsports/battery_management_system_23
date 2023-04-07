/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "alerts.h"
#include "cellData.h"
#include "packData.h"


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

static bool overvoltagePresent(Bms_S* bms)
{
    return (bms->maxBrickV > MAX_BRICK_VOLTAGE);
}

static bool undervoltagePresent(Bms_S* bms)
{
    return (bms->minBrickV < MIN_BRICK_VOLTAGE);
}

static bool cellImbalancePresent(Bms_S* bms)
{
    float maxCellImbalanceV = bms->maxBrickV - bms->minBrickV;

    return (maxCellImbalanceV > MAX_CELL_IMBALANCE_V);
}

static bool overtemperatureWarningPresent(Bms_S* bms)
{
    return (bms->maxBrickTemp > MAX_BRICK_TEMP_WARNING_C);
}

static bool overtemperatureFaultPresent(Bms_S* bms)
{
    return (bms->maxBrickTemp > MAX_BRICK_TEMP_FAULT_C);
}

static bool amsSdcFaultPresent(Bms_S* bms)
{
    return HAL_GPIO_ReadPin(AMS_FAULT_SDC_GPIO_Port, AMS_FAULT_SDC_Pin);
}

static bool bspdSdcFaultPresent(Bms_S* bms)
{
    return HAL_GPIO_ReadPin(BSPD_FAULT_SDC_GPIO_Port, BSPD_FAULT_SDC_Pin);
}

static bool imdSdcFaultPresent(Bms_S* bms)
{
    return HAL_GPIO_ReadPin(IMD_FAULT_SDC_GPIO_Port, IMD_FAULT_SDC_Pin);
}

static bool currentSensorErrorPresent(Bms_S* bms)
{
    // TODO: Implement current sense error check
    return false;
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Get the status of any given alert
  @param   alert - The alert data structure whose status to read
  @return  The current status of the alert
*/
AlertStatus_E alertStatus(Alert_S* alert)
{
    return alert->alertStatus;
}

/*!
  @brief   Run the alert monitor to update the status of the alert
  @param   bms - The BMS data structure
  @param   alert - The Alert data structure
*/
void runAlertMonitor(Bms_S* bms, Alert_S* alert)
{
    if (alert->alertStatus == ALERT_CLEARED)
    {
        // Determine if we need to set the alert
        if (alert->alertConditionPresent(bms))
        {
            // Increment alert timer by 10ms
            updateTimer(&alert->alertTimer);
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
            updateTimer(&alert->alertTimer);
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


/* ==================================================================== */
/* ========================= GLOBAL VARIABLES ========================= */
/* ==================================================================== */

Alert_S overvoltageAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERVOLTAGE_ALERT_SET_TIME_MS}, 
                            .setTime_MS = OVERVOLTAGE_ALERT_SET_TIME_MS, .clearTime_MS = OVERVOLTAGE_ALERT_CLEAR_TIME_MS, 
                            .alertConditionPresent = overvoltagePresent};

Alert_S undervoltageAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, UNDERVOLTAGE_ALERT_SET_TIME_MS}, 
                             .setTime_MS = UNDERVOLTAGE_ALERT_SET_TIME_MS, .clearTime_MS = UNDERVOLTAGE_ALERT_CLEAR_TIME_MS, 
                             .alertConditionPresent = undervoltagePresent};

Alert_S cellImbalanceAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, CELL_IMBALANCE_ALERT_SET_TIME_MS}, 
                              .setTime_MS = CELL_IMBALANCE_ALERT_SET_TIME_MS, .clearTime_MS = CELL_IMBALANCE_ALERT_CLEAR_TIME_MS, 
                              .alertConditionPresent = cellImbalancePresent};

Alert_S overtemperatureWarningAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS}, 
                                       .setTime_MS = OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS, .clearTime_MS = OVERTEMPERATURE_WARNING_ALERT_CLEAR_TIME_MS, 
                                       .alertConditionPresent = overtemperatureWarningPresent};

Alert_S overtemperatureFaultAlert   = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS}, 
                                       .setTime_MS = OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = OVERTEMPERATURE_FAULT_ALERT_CLEAR_TIME_MS, 
                                       .alertConditionPresent = overtemperatureFaultPresent};

Alert_S amsSdcFaultAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
                            .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
                            .alertConditionPresent = amsSdcFaultPresent};

Alert_S bspdSdcFaultAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
                             .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
                             .alertConditionPresent = bspdSdcFaultPresent};

Alert_S imdSdcFaultAlert = {.alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
                            .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
                            .alertConditionPresent = imdSdcFaultPresent};
