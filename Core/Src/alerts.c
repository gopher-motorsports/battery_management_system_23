/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "alerts.h"
#include "cellData.h"
#include "packData.h"


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

static bool overvoltageWarningPresent(Bms_S* bms)
{
    return (bms->maxBrickV > MAX_BRICK_WARNING_VOLTAGE);
}

static bool overvoltageFaultPresent(Bms_S* bms)
{
    return (bms->maxBrickV > MAX_BRICK_FAULT_VOLTAGE);
}

static bool undervoltageWarningPresent(Bms_S* bms)
{
    return (bms->minBrickV < MIN_BRICK_WARNING_VOLTAGE);
}

static bool undervoltageFaultPresent(Bms_S* bms)
{
    return (bms->minBrickV < MIN_BRICK_FAULT_VOLTAGE);
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

// static bool currentSensorErrorPresent(Bms_S* bms)
// {
//     // TODO: Implement current sense error check
//     return false;
// }


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Get the status of any given alert
  @param   alert - The alert data structure whose status to read
  @return  The current status of the alert
*/
AlertStatus_E getAlertStatus(Alert_S* alert)
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

// Overvoltage Warning Alert
const AlertResponse_E overvoltageWarningAlertResponse[] = { DISABLE_CHARGING };
#define NUM_OVERVOLTAGE_WARNING_ALERT_RESPONSE sizeof(overvoltageWarningAlertResponse) / sizeof(AlertResponse_E)
Alert_S overvoltageWarningAlert =
{ 
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
    .setTime_MS = OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS, .clearTime_MS = OVERVOLTAGE_WARNING_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = overvoltageWarningPresent, 
    .numAlertResponse = NUM_OVERVOLTAGE_WARNING_ALERT_RESPONSE, .alertResponse =  overvoltageWarningAlertResponse
};

// Undervoltage Warning Alert
const AlertResponse_E undervoltageWarningAlertResponse[] = { LIMP_MODE };
#define NUM_UNDERVOLTAGE_WARNING_ALERT_RESPONSE sizeof(undervoltageWarningAlertResponse) / sizeof(AlertResponse_E)
Alert_S undervoltageWarningAlert = 
{ 
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
    .setTime_MS = UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS, .clearTime_MS = UNDERVOLTAGE_WARNING_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = undervoltageWarningPresent,
    .numAlertResponse = NUM_UNDERVOLTAGE_WARNING_ALERT_RESPONSE, .alertResponse = undervoltageWarningAlertResponse
};

// Overvoltage Fault Alert
const AlertResponse_E overvoltageFaultAlertResponse[] = { DISABLE_CHARGING, EMERGENCY_BLEED, AMS_FAULT};
#define NUM_OVERVOLTAGE_FAULT_ALERT_RESPONSE sizeof(overvoltageFaultAlertResponse) / sizeof(AlertResponse_E)
Alert_S overvoltageFaultAlert = 
{ 
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
    .setTime_MS = OVERVOLTAGE_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = OVERVOLTAGE_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = overvoltageFaultPresent, 
    .numAlertResponse = NUM_OVERVOLTAGE_FAULT_ALERT_RESPONSE, .alertResponse =  overvoltageFaultAlertResponse
};

// Undervoltage Fault Alert
const AlertResponse_E undervoltageFaultAlertResponse[] = { AMS_FAULT };
#define NUM_UNDERVOLTAGE_FAULT_ALERT_RESPONSE sizeof(undervoltageFaultAlertResponse) / sizeof(AlertResponse_E)
Alert_S undervoltageFaultAlert = 
{ 
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
    .setTime_MS = UNDERVOLTAGE_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = UNDERVOLTAGE_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = undervoltageFaultPresent,
    .numAlertResponse = NUM_UNDERVOLTAGE_FAULT_ALERT_RESPONSE, .alertResponse = undervoltageFaultAlertResponse
};

// Cell Imbalance Alert
const AlertResponse_E cellImbalanceAlertResponse[] = {LIMP_MODE, DISABLE_CHARGING, DISABLE_BALANCING };
#define NUM_CELL_IMBALANCE_ALERT_RESPONSE sizeof(cellImbalanceAlertResponse) / sizeof(AlertResponse_E)
Alert_S cellImbalanceAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, CELL_IMBALANCE_ALERT_SET_TIME_MS}, 
    .setTime_MS = CELL_IMBALANCE_ALERT_SET_TIME_MS, .clearTime_MS = CELL_IMBALANCE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = cellImbalancePresent,
    .numAlertResponse = NUM_CELL_IMBALANCE_ALERT_RESPONSE, .alertResponse = cellImbalanceAlertResponse
};

// Overtemperature Warning Alert
const AlertResponse_E overtempWarningAlertResponse[] = { LIMP_MODE, DISABLE_CHARGING, DISABLE_BALANCING };
#define NUM_OVERTEMP_WARNING_ALERT_RESPONSE sizeof(overtempWarningAlertResponse) / sizeof(AlertResponse_E)
Alert_S overtempWarningAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS}, 
    .setTime_MS = OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS, .clearTime_MS = OVERTEMPERATURE_WARNING_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = overtemperatureWarningPresent,
    .numAlertResponse = NUM_OVERTEMP_WARNING_ALERT_RESPONSE, .alertResponse = overtempWarningAlertResponse
};

// Overtemperature Fault Alert
const AlertResponse_E overtempFaultAlertResponse[] = { DISABLE_CHARGING, DISABLE_BALANCING, AMS_FAULT };
#define NUM_OVERTEMP_FAULT_ALERT_RESPONSE sizeof(overtempFaultAlertResponse) / sizeof(AlertResponse_E)
Alert_S overtempFaultAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS}, 
    .setTime_MS = OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = OVERTEMPERATURE_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = overtemperatureFaultPresent,
    .numAlertResponse = NUM_OVERTEMP_FAULT_ALERT_RESPONSE, .alertResponse = overtempFaultAlertResponse
};

// AMS Shut Down Circuit Alert
const AlertResponse_E amsSdcAlertResponse[] = { INFO_ONLY };
#define NUM_AMS_SDC_ALERT_RESPONSE sizeof(amsSdcAlertResponse) / sizeof(AlertResponse_E)
Alert_S amsSdcFaultAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
    .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = amsSdcFaultPresent,
    .numAlertResponse = NUM_AMS_SDC_ALERT_RESPONSE, .alertResponse = amsSdcAlertResponse
};

// BSPD Shut Down Circuit Alert
const AlertResponse_E bspdSdcAlertResponse[] = { INFO_ONLY };
#define NUM_BSPD_SDC_ALERT_RESPONSE sizeof(bspdSdcAlertResponse) / sizeof(AlertResponse_E)
Alert_S bspdSdcFaultAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
    .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = bspdSdcFaultPresent,
    .numAlertResponse = NUM_BSPD_SDC_ALERT_RESPONSE, .alertResponse = bspdSdcAlertResponse
};

// IMD Shut Down Circuit Alert
const AlertResponse_E imdSdcAlertResponse[] = { INFO_ONLY };
#define NUM_IMD_SDC_ALERT_RESPONSE sizeof(imdSdcAlertResponse) / sizeof(AlertResponse_E)
Alert_S imdSdcFaultAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
    .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = imdSdcFaultPresent,
    .numAlertResponse = NUM_IMD_SDC_ALERT_RESPONSE, .alertResponse = imdSdcAlertResponse
};

// Alert - bad current sensor

// Alert - lost cell tap

// Alert - lost BMB comms

// Alert - lost more than 60% of temp sensors in a pack

// Alert - TBD stuck open/closed bleed fet


Alert_S* alerts[] = 
{
    &overvoltageWarningAlert,
    &undervoltageWarningAlert,
    &overvoltageFaultAlert,
    &undervoltageFaultAlert,
    &cellImbalanceAlert,
    &overtempWarningAlert,
    &overtempFaultAlert,
    &amsSdcFaultAlert,
    &bspdSdcFaultAlert,
    &imdSdcFaultAlert
};

// Number of alerts
const uint32_t NUM_ALERTS = sizeof(alerts) / sizeof(Alert_S*);
