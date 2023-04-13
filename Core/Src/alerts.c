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
    const float maxCellImbalanceV = bms->maxBrickV - bms->minBrickV;

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

static bool badVoltageSensorStatusPresent(Bms_S* bms)
{
    return false;
}

static bool badBrickTempSensorStatusPresent(Bms_S* bms)
{
    return false;
}

static bool badBoardTempSensorStatusPresent(Bms_S* bms)
{
    return false;
}

static bool insufficientTempSensePresent(Bms_S* bms)
{
    return false;
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
        if (checkTimerExpired(&alert->alertTimer) && !(alert->alertTimer.timThreshold <= 0 && !alert->alertConditionPresent(bms)))
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
        if (checkTimerExpired(&alert->alertTimer) && !(alert->alertTimer.timThreshold <= 0 && alert->alertConditionPresent(bms)))
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
    .alertName = "OvervoltageWarning",
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
    .alertName = "UndervoltageWarning",
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
    .alertName = "OvervoltageFault",
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
    .alertName = "UndervoltageFault",
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
    .alertName = "CellImbalance",
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
    .alertName = "OvertempWarning",
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
    .alertName = "OvertempFault",
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
    .alertName = "AmsSdcLatched",
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
    .alertName = "BspdSdcLatched",
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
    .alertName = "ImdSdcLatched",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, SDC_FAULT_ALERT_SET_TIME_MS}, 
    .setTime_MS = SDC_FAULT_ALERT_SET_TIME_MS, .clearTime_MS = SDC_FAULT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = imdSdcFaultPresent,
    .numAlertResponse = NUM_IMD_SDC_ALERT_RESPONSE, .alertResponse = imdSdcAlertResponse
};

// Bad Current Sensor Alert
const AlertResponse_E currentSensorErrorAlertResponse[] = { DISABLE_CHARGING };
#define NUM_CURRENT_SENSOR_ERROR_ALERT_RESPONSE sizeof(currentSensorErrorAlertResponse) / sizeof(AlertResponse_E)
Alert_S currentSensorErrorAlert = 
{
    .alertName = "BadCurrentSense",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, CURRENT_SENSOR_ERROR_ALERT_SET_TIME_MS}, 
    .setTime_MS = CURRENT_SENSOR_ERROR_ALERT_SET_TIME_MS, .clearTime_MS = CURRENT_SENSOR_ERROR_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = currentSensorErrorPresent,
    .numAlertResponse = NUM_CURRENT_SENSOR_ERROR_ALERT_RESPONSE, .alertResponse = currentSensorErrorAlertResponse
};

// Alert - lost cell tap

// Lost BMB communications Alert
const AlertResponse_E bmbCommunicationFailureAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_BMB_COMMUNICATION_FAILURE_ALERT_RESPONSE sizeof(bmbCommunicationFailureAlertResponse) / sizeof(AlertResponse_E)
Alert_S bmbCommunicationFailureAlert = 
{
    .alertName = "BmbCommunicationFailure",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, BMB_COMMUNICATION_FAILURE_ALERT_SET_TIME_MS}, 
    .setTime_MS = BMB_COMMUNICATION_FAILURE_ALERT_SET_TIME_MS, .clearTime_MS = BMB_COMMUNICATION_FAILURE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = currentSensorErrorPresent,
    .numAlertResponse = NUM_BMB_COMMUNICATION_FAILURE_ALERT_RESPONSE, .alertResponse = bmbCommunicationFailureAlertResponse
};

// Bad voltage sensor status
const AlertResponse_E badVoltageSenseStatusAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_BAD_VOLTAGE_SENSE_STATUS_ALERT_RESPONSE sizeof(badVoltageSenseStatusAlertResponse) / sizeof(AlertResponse_E)
Alert_S badVoltageSenseStatusAlert = 
{
    .alertName = "BadVoltageSenseStatus",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, BAD_VOLTAGE_SENSE_STATUS_ALERT_SET_TIME_MS}, 
    .setTime_MS = BAD_VOLTAGE_SENSE_STATUS_ALERT_SET_TIME_MS, .clearTime_MS = BAD_VOLTAGE_SENSE_STATUS_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = badVoltageSensorStatusPresent,
    .numAlertResponse = NUM_BAD_VOLTAGE_SENSE_STATUS_ALERT_RESPONSE, .alertResponse = badVoltageSenseStatusAlertResponse
};

// Bad brick temperature sensor status
const AlertResponse_E badBrickTempSenseStatusAlertResponse[] = { INFO_ONLY };
#define NUM_BAD_BRICK_TEMP_SENSE_STATUS_ALERT_RESPONSE sizeof(badBrickTempSenseStatusAlertResponse) / sizeof(AlertResponse_E)
Alert_S badBrickTempSenseStatusAlert = 
{
    .alertName = "BadBrickTempSenseStatus",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, BAD_BRICK_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS}, 
    .setTime_MS = BAD_BRICK_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS, .clearTime_MS = BAD_BRICK_TEMP_SENSE_STATUS_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = badBrickTempSensorStatusPresent,
    .numAlertResponse = NUM_BAD_BRICK_TEMP_SENSE_STATUS_ALERT_RESPONSE, .alertResponse = badBrickTempSenseStatusAlertResponse
};

// Bad board temperature sensor status
const AlertResponse_E badBoardTempSenseStatusAlertResponse[] = { INFO_ONLY };
#define NUM_BAD_BOARD_TEMP_SENSE_STATUS_ALERT_RESPONSE sizeof(badBoardTempSenseStatusAlertResponse) / sizeof(AlertResponse_E)
Alert_S badBoardTempSenseStatusAlert = 
{
    .alertName = "BadBoardTempSenseStatus",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, BAD_BOARD_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS}, 
    .setTime_MS = BAD_BOARD_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS, .clearTime_MS = BAD_BOARD_TEMP_SENSE_STATUS_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = badBoardTempSensorStatusPresent,
    .numAlertResponse = NUM_BAD_BOARD_TEMP_SENSE_STATUS_ALERT_RESPONSE, .alertResponse = badBoardTempSenseStatusAlertResponse
};

// Lost more than 60% of temp sensors in pack
const AlertResponse_E insufficientTempSensorsAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_INSUFFICIENT_TEMP_SENSORS_ALERT_RESPONSE sizeof(insufficientTempSensorsAlertResponse) / sizeof(AlertResponse_E)
Alert_S insufficientTempSensorsAlert = 
{
    .alertName = "InsufficientTempSensors",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, INSUFFICIENT_TEMP_SENSOR_ALERT_SET_TIME_MS}, 
    .setTime_MS = INSUFFICIENT_TEMP_SENSOR_ALERT_SET_TIME_MS, .clearTime_MS = INSUFFICIENT_TEMP_SENSOR_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = insufficientTempSensePresent,
    .numAlertResponse = NUM_INSUFFICIENT_TEMP_SENSORS_ALERT_RESPONSE, .alertResponse = insufficientTempSensorsAlertResponse
};

// Alert - TBD stuck open/closed bleed fet

// Stack vs Segment Voltage Imbalance Alert
const AlertResponse_E stackVsSegmentImbalanceAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING };
#define NUM_STACK_VS_SEGMENT_IMBALANCE_ALERT_RESPONSE sizeof(stackVsSegmentImbalanceAlertResponse) / sizeof(AlertResponse_E)
Alert_S stackVsSegmentImbalanceAlert = 
{
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){0, STACK_VS_SEGMENT_IMBALANCE_ALERT_SET_TIME_MS}, 
    .setTime_MS = STACK_VS_SEGMENT_IMBALANCE_ALERT_SET_TIME_MS, .clearTime_MS = STACK_VS_SEGMENT_IMBALANCE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = currentSensorErrorPresent,
    .numAlertResponse = NUM_STACK_VS_SEGMENT_IMBALANCE_ALERT_RESPONSE, .alertResponse = stackVsSegmentImbalanceAlertResponse
};

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
    &imdSdcFaultAlert,
    &currentSensorErrorAlert,
    &bmbCommunicationFailureAlert,
    &badVoltageSenseStatusAlert,
    &badBrickTempSenseStatusAlert,
    &badBoardTempSenseStatusAlert,
    &insufficientTempSensorsAlert,
    &stackVsSegmentImbalanceAlert
};

// Number of alerts
const uint32_t NUM_ALERTS = sizeof(alerts) / sizeof(Alert_S*);
