/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdint.h>
#include "main.h"
#include "debug.h"
#include "charger.h"
#include <math.h>
#include "utils.h"

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern CAN_HandleTypeDef hcan1;
extern volatile uint8_t chargerMessage[5];

/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/*!
  @brief   Send a CAN message to the charger requesting a charger enable or disable
  @param   chargerState - A request to disable or enable the charger
*/
static void sendChargerRequest(Charger_State_E chargerState)
{
    // Create template for EXT frame CAN message 
    CAN_TxHeaderTypeDef TxHeader;
    uint8_t TxData[8] = {0};            // Default data frame is all zeros
    uint32_t TxMailbox;

    TxHeader.IDE = CAN_ID_EXT;          // Extended CAN ID
    TxHeader.ExtId = CHARGER_CAN_ID;    // Charger CAN ID
    TxHeader.RTR = CAN_RTR_DATA;        // Sending data frame
    TxHeader.DLC = 8;                   // 8 Bytes of data

    if(chargerState == CHARGER_ENABLE)
    {
        // Encode voltage and current requests
        // [0] Voltage*10 HIGH Byte
        // [1] Voltage*10 LOW Byte
        // [2] Current*10 HIGH Byte
        // [3] Current*10 LOW Byte
        uint16_t deciVolts = (uint16_t)(MAX_CELL_VOLTAGE_THRES_LOW * 10);
        uint16_t deciAmps = (uint16_t)(MAX_CHRAGE_CURRENT * 10);

        TxData[0] = (uint8_t)(deciVolts >> 8);  
        TxData[1] = (uint8_t)(deciVolts);

        TxData[2] = (uint8_t)(deciAmps >> 8);  
        TxData[3] = (uint8_t)(deciAmps);
    }
    else
    {
        // Disable Charging
        TxData[4] = 1;
    }

    // Send CAN message to Charger
    if (HAL_CAN_AddTxMessage(&hcan1, &TxHeader, TxData, &TxMailbox) != HAL_OK)
    {
        Debug("Failed to send message to charger\n");
    }
}

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Determine if the charger should be enabled or disabled, and send a request to the charger
  @param   bms - BMS data struct
*/
void updateChargerState(Bms_S* bms)
{
    // Calculate the max cell imbalance in the accumulator
    float packImbalance = bms->maxBrickV - bms->minBrickV;

    // Disable charging if either the cell imbalance or the max cell voltage meet/exceed their corresponding high threshold
    // Wait for cell balancing to correct imbalances
    if ((packImbalance >= MAX_CELL_IMBALANCE_THRES_HIGH) || (bms->maxBrickV >= MAX_CELL_VOLTAGE_THRES_HIGH))
    {
        bms->chargerState = CHARGER_DISABLE;
    }
    // Enable charging if both the cell imbalance and the max cell voltage have met/are under their corresponding low threshold
    else if ((packImbalance <= MAX_CELL_IMBALANCE_THRES_LOW) && (bms->maxBrickV <= MAX_CELL_VOLTAGE_THRES_LOW))
    {
        bms->chargerState = CHARGER_ENABLE;
    }

    // Send the current requested charger state to the charger over CAN
    sendChargerRequest(bms->chargerState);
}

/*!
  @brief   Validate that the recieved charger CAN message indicates no faults and corresponds to the current accumulator status
  @param   bms - BMS data struct
*/
void validateChargerState(Bms_S* bms)
{
    // Decode charger CAN message into output voltage and current 
    // [0] Voltage*10 HIGH Byte
    // [1] Voltage*10 LOW Byte
    // [2] Current*10 HIGH Byte
    // [3] Current*10 LOW Byte
    uint16_t deciVolts = (uint16_t)chargerMessage[0] << 8 | (uint16_t)chargerMessage[1];
    uint16_t deciAmps = (uint16_t)chargerMessage[2] << 8 | (uint16_t)chargerMessage[3];
    float chargerVoltage = (float)deciVolts / 10.0f;
    float chargerCurrent = (float)deciAmps / 10.0f;

    // Validate that the charger output voltage matches the accumulator's measured voltage
    float accumulatorVoltage = bms->avgBrickV * NUM_BMBS_IN_ACCUMULATOR * NUM_BRICKS_PER_BMB;
    if (fabs(accumulatorVoltage - chargerVoltage) > CHARGER_VOLTAGE_THRESHOLD)
    {
        bms->chargerStatus = CHARGER_VOLTAGE_FAULT;
        return;
    }

    // Validate that the charger output current matches the accumulator's measured current
    float accumulatorCurrent = bms->tractiveSystemCurrent;
    if (fabs(accumulatorCurrent - chargerCurrent) > CHARGER_CURRENT_THRESHOLD)
    {
        bms->chargerStatus = CHARGER_CURRENT_FAULT;
        return;
    }

    // Shift through the charger status byte and check for charger errors
    uint8_t statusByte = chargerMessage[4];
    uint8_t mask = 0x80;
    for (int32_t i = CHARGER_HARDWARE_FAULT; i < NUM_CHARGER_FAULTS; i++)
    {
        if ((statusByte & mask) != 0x00)
        {
            bms->chargerState = i;
            return;
        }
        mask >>= 1;
    }
}
