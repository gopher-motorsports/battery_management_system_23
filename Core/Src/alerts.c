/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "alerts.h"
#include "cellData.h"
#include "packData.h"
#include "charger.h"
#include <math.h>

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
    for (uint32_t i = 0; i < bms->numBmbs; i++)
    {
        Bmb_S* bmb = &bms->bmb[i];
        if (bmb->numBadBrickV != 0)
        {
            return true;
        }
    }
    return false;
}

static bool badBrickTempSensorStatusPresent(Bms_S* bms)
{
    for (uint32_t i = 0; i < bms->numBmbs; i++)
    {
        Bmb_S* bmb = &bms->bmb[i];
        if (bmb->numBadBrickTemp != 0)
        {
            return true;
        }
    }
    return false;
}

static bool badBoardTempSensorStatusPresent(Bms_S* bms)
{
    for (uint32_t i = 0; i < bms->numBmbs; i++)
    {
        Bmb_S* bmb = &bms->bmb[i];
        if (bmb->numBadBoardTemp != 0)
        {
            return true;
        }
    }
    return false;
}

static bool insufficientTempSensePresent(Bms_S* bms)
{
    for (uint32_t i = 0; i < bms->numBmbs; i++)
    {
        Bmb_S* bmb = &bms->bmb[i];
        const uint32_t maxNumBadBrickTempAllowed = NUM_BRICKS_PER_BMB * (100 - MIN_PERCENT_BRICK_TEMPS_MONITORED) / 100;
        if (bmb->numBadBrickTemp > maxNumBadBrickTempAllowed)
        {
            return true;
        }
    }
    return false;
}

static bool currentSensorErrorPresent(Bms_S* bms)
{
    // TODO: Implement current sense error check
    return false;
}

static bool bmbCommunicationFailurePresent(Bms_S* bms)
{
    // STUB
    return false;
}

static bool stackVsSegmentImbalancePresent(Bms_S* bms)
{
    // STUB
    return false;
}

static bool chargerOverVoltagePresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerVoltage > MAX_CHARGE_VOLTAGE_V + CHARGER_VOLTAGE_MISMATCH_THRESHOLD);
}

static bool chargerOverCurrentPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && ((bms->chargerData.chargerCurrent > MAX_CHARGE_CURRENT_A + CHARGER_CURRENT_MISMATCH_THRESHOLD)  || (bms->tractiveSystemCurrent > MAX_CHARGE_CURRENT_A + CHARGER_CURRENT_MISMATCH_THRESHOLD));
}

static bool chargerVoltageMismatchPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (fabsf(bms->accumulatorVoltage - bms->chargerData.chargerVoltage) > CHARGER_VOLTAGE_MISMATCH_THRESHOLD);
}

static bool chargerCurrentMismatchPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (fabsf(bms->tractiveSystemCurrent - bms->chargerData.chargerCurrent) > CHARGER_CURRENT_MISMATCH_THRESHOLD);
}

static bool chargerHardwareFailurePresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerStatus[CHARGER_HARDWARE_FAILURE_ERROR]);
}

static bool chargerOverTempPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerStatus[CHARGER_OVERTEMP_ERROR]);
}

static bool chargerInputVoltageErrorPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerStatus[CHARGER_INPUT_VOLTAGE_ERROR]);
}

static bool chargerBatteryNotDetectedErrorPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerStatus[CHRAGER_BATTERY_NOT_DETECTED_ERROR]);
}

static bool chargerCommunicationErrorPresent(Bms_S* bms)
{
    return (bms->chargerConnected) && (bms->chargerData.chargerStatus[CHARGER_COMMUNICATION_ERROR]);
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
    if (alert->alertStatus == ALERT_CLEARED || alert->alertStatus == ALERT_LATCHED)
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

        // Determine if alert was set or in the case that the timer threshold is 0 then check whether the alert condition is present
        if (checkTimerExpired(&alert->alertTimer) && (!(alert->alertTimer.timThreshold <= 0) || alert->alertConditionPresent(bms)))
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

        // Determine if alert was cleared or in the case that the timer threshold is 0 then check whether the alert condition is not present
        if (checkTimerExpired(&alert->alertTimer) && (!(alert->alertTimer.timThreshold <= 0) || !alert->alertConditionPresent(bms)))
        {
            // Timer expired indicating alert is no longer present. Either set status to latched or clear
            if (alert->latching)
            {
                // Latching alerts can't be cleared - set status to latched to indicate that conditions are no longer met
                alert->alertStatus = ALERT_LATCHED;
            }
            else
            {
                // If non latching alert, the alert can be cleared
                alert->alertStatus = ALERT_CLEARED;
            }
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = OVERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = UNDERVOLTAGE_WARNING_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CELL_IMBALANCE_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = OVERTEMPERATURE_WARNING_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = OVERTEMPERATURE_FAULT_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = SDC_FAULT_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = SDC_FAULT_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = SDC_FAULT_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CURRENT_SENSOR_ERROR_ALERT_SET_TIME_MS}, 
    .setTime_MS = CURRENT_SENSOR_ERROR_ALERT_SET_TIME_MS, .clearTime_MS = CURRENT_SENSOR_ERROR_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = currentSensorErrorPresent,
    .numAlertResponse = NUM_CURRENT_SENSOR_ERROR_ALERT_RESPONSE, .alertResponse = currentSensorErrorAlertResponse
};

// Lost BMB communications Alert
const AlertResponse_E bmbCommunicationFailureAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_BMB_COMMUNICATION_FAILURE_ALERT_RESPONSE sizeof(bmbCommunicationFailureAlertResponse) / sizeof(AlertResponse_E)
Alert_S bmbCommunicationFailureAlert = 
{
    .alertName = "BmbCommunicationFailure",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = BMB_COMMUNICATION_FAILURE_ALERT_SET_TIME_MS}, 
    .setTime_MS = BMB_COMMUNICATION_FAILURE_ALERT_SET_TIME_MS, .clearTime_MS = BMB_COMMUNICATION_FAILURE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = bmbCommunicationFailurePresent,
    .numAlertResponse = NUM_BMB_COMMUNICATION_FAILURE_ALERT_RESPONSE, .alertResponse = bmbCommunicationFailureAlertResponse
};

// Bad voltage sensor status
const AlertResponse_E badVoltageSenseStatusAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_BAD_VOLTAGE_SENSE_STATUS_ALERT_RESPONSE sizeof(badVoltageSenseStatusAlertResponse) / sizeof(AlertResponse_E)
Alert_S badVoltageSenseStatusAlert = 
{
    .alertName = "BadVoltageSenseStatus",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = BAD_VOLTAGE_SENSE_STATUS_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = BAD_BRICK_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = BAD_BOARD_TEMP_SENSE_STATUS_ALERT_SET_TIME_MS}, 
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
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = INSUFFICIENT_TEMP_SENSOR_ALERT_SET_TIME_MS}, 
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
    .alertName = "StackVsSegmentImbalance",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = STACK_VS_SEGMENT_IMBALANCE_ALERT_SET_TIME_MS}, 
    .setTime_MS = STACK_VS_SEGMENT_IMBALANCE_ALERT_SET_TIME_MS, .clearTime_MS = STACK_VS_SEGMENT_IMBALANCE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = stackVsSegmentImbalancePresent,
    .numAlertResponse = NUM_STACK_VS_SEGMENT_IMBALANCE_ALERT_RESPONSE, .alertResponse = stackVsSegmentImbalanceAlertResponse
};

// Charger Overvoltage Alert
const AlertResponse_E chargerOverVoltageAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_OVERVOLTAGE_ALERT_RESPONSE sizeof(chargerOverVoltageAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerOverVoltageAlert = 
{
    .alertName = "ChargerOvervoltage",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_OVERVOLTAGE_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_OVERVOLTAGE_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_OVERVOLTAGE_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerOverVoltagePresent,
    .numAlertResponse = NUM_CHARGER_OVERVOLTAGE_ALERT_RESPONSE, .alertResponse = chargerOverVoltageAlertResponse
};

// Charger Overcurrent Alert
const AlertResponse_E chargerOverCurrentAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_OVERCURRENT_ALERT_RESPONSE sizeof(chargerOverCurrentAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerOverCurrentAlert = 
{
    .alertName = "ChargerOvercurrent",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_OVERCURRENT_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_OVERCURRENT_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_OVERCURRENT_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerOverCurrentPresent,
    .numAlertResponse = NUM_CHARGER_OVERCURRENT_ALERT_RESPONSE, .alertResponse = chargerOverCurrentAlertResponse
};

// Accumulator Voltage vs Charger Voltage Mismatch Alert
const AlertResponse_E chargerVoltageMismatchAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_VOLTAGE_MISMATCH_ALERT_RESPONSE sizeof(chargerVoltageMismatchAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerVoltageMismatchAlert = 
{
    .alertName = "ChargerVoltageMismatch",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_VOLTAGE_MISMATCH_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_VOLTAGE_MISMATCH_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_VOLTAGE_MISMATCH_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerVoltageMismatchPresent,
    .numAlertResponse = NUM_CHARGER_VOLTAGE_MISMATCH_ALERT_RESPONSE, .alertResponse = chargerVoltageMismatchAlertResponse
};

// Accumulator Current vs Charger Current Mismatch Alert
const AlertResponse_E chargerCurrentMismatchAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_CURRENT_MISMATCH_ALERT_RESPONSE sizeof(chargerCurrentMismatchAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerCurrentMismatchAlert = 
{
    .alertName = "ChargerCurrentMismatch",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_CURRENT_MISMATCH_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_CURRENT_MISMATCH_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_CURRENT_MISMATCH_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerCurrentMismatchPresent,
    .numAlertResponse = NUM_CHARGER_CURRENT_MISMATCH_ALERT_RESPONSE, .alertResponse = chargerCurrentMismatchAlertResponse
};

// Charger Hardware Failure Alert
const AlertResponse_E chargerHardwareFailureAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_HARDWARE_FAILURE_ALERT_RESPONSE sizeof(chargerHardwareFailureAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerHardwareFailureAlert = 
{
    .alertName = "ChargerHardwareFailure",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerHardwareFailurePresent,
    .numAlertResponse = NUM_CHARGER_HARDWARE_FAILURE_ALERT_RESPONSE, .alertResponse = chargerHardwareFailureAlertResponse
};

// Charger Overtemp Alert
const AlertResponse_E chargerOverTempAlertResponse[] = { DISABLE_BALANCING, DISABLE_CHARGING, AMS_FAULT };
#define NUM_CHARGER_OVER_TEMP_ALERT_RESPONSE sizeof(chargerOverTempAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerOverTempAlert = 
{
    .alertName = "ChargerOverTemp",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerOverTempPresent,
    .numAlertResponse = NUM_CHARGER_OVER_TEMP_ALERT_RESPONSE, .alertResponse = chargerOverTempAlertResponse
};

// Charger Input Voltage Error Alert
const AlertResponse_E chargerInputVoltageErrorAlertResponse[] = { INFO_ONLY };
#define NUM_CHARGER_INPUT_VOLTAGE_ERROR_ALERT_RESPONSE sizeof(chargerInputVoltageErrorAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerInputVoltageErrorAlert = 
{
    .alertName = "ChargerInputVoltageError",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerInputVoltageErrorPresent,
    .numAlertResponse = NUM_CHARGER_INPUT_VOLTAGE_ERROR_ALERT_RESPONSE, .alertResponse = chargerInputVoltageErrorAlertResponse
};

// Charger Battery Not Detected Error Alert
const AlertResponse_E chargerBatteryNotDetectedErrorAlertResponse[] = { INFO_ONLY };
#define NUM_CHARGER_BATTERY_NOT_DETECTED_ERROR_ALERT_RESPONSE sizeof(chargerBatteryNotDetectedErrorAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerBatteryNotDetectedErrorAlert = 
{
    .alertName = "ChargerVoltageNotDetected",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerBatteryNotDetectedErrorPresent,
    .numAlertResponse = NUM_CHARGER_BATTERY_NOT_DETECTED_ERROR_ALERT_RESPONSE, .alertResponse = chargerBatteryNotDetectedErrorAlertResponse
};

// Charger Communication Error Alert
const AlertResponse_E chargerCommunicationErrorAlertResponse[] = { INFO_ONLY };
#define NUM_CHARGER_COMMUNICATION_ERROR_ALERT_RESPONSE sizeof(chargerCommunicationErrorAlertResponse) / sizeof(AlertResponse_E)
Alert_S chargerCommunicationErrorAlert = 
{
    .alertName = "ChargerCommsFailure",
    .alertStatus = ALERT_CLEARED, .alertTimer = (Timer_S){.timCount = 0, .lastUpdate = 0, .timThreshold = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS}, 
    .setTime_MS = CHARGER_DIAGNOSTIC_ALERT_SET_TIME_MS, .clearTime_MS = CHARGER_DIAGNOSTIC_ALERT_CLEAR_TIME_MS, 
    .alertConditionPresent = chargerCommunicationErrorPresent,
    .numAlertResponse = NUM_CHARGER_COMMUNICATION_ERROR_ALERT_RESPONSE, .alertResponse = chargerCommunicationErrorAlertResponse
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
    &stackVsSegmentImbalanceAlert,
    &chargerOverVoltageAlert,
    &chargerOverCurrentAlert,
    &chargerVoltageMismatchAlert,
    &chargerCurrentMismatchAlert,
    &chargerHardwareFailureAlert,
    &chargerOverTempAlert,
    &chargerInputVoltageErrorAlert,
    &chargerBatteryNotDetectedErrorAlert,
    &chargerCommunicationErrorAlert
};

// Number of alerts
const uint32_t NUM_ALERTS = sizeof(alerts) / sizeof(Alert_S*);
