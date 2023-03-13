/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "currentSense.h"
#include "GopherCAN.h"
#include <math.h>
#include "debug.h"

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Update the bms tractive system current data and sensor status from gophercan
  @param   bms - BMS data struct
*/
void getTractiveSystemCurrent(Bms_S* bms)
{
    // Fetch current sensor data from gophercan
    float currHI = bmsTractiveSystemCurrentHigh_A.data;
    float currLO = bmsTractiveSystemCurrentLow_A.data;

    // If the current exceeds the following threshold in either the positive or negative direction,
    // the sensor input has railed to 0 or 5v and a current sensor error is set
    bms->currentSensorStatusHI = (fabs(currHI) < CURRENT_HIGH_RAIL_THRESHOLD) ? (SENSE_GOOD) : (SENSE_MIA);
    bms->currentSensorStatusLO = (fabs(currLO) < CURRENT_LOW_RAIL_THRESHOLD) ? (SENSE_GOOD) : (SENSE_MIA);

    // To use the HI current sensor channel, it must be working AND (it must exceed the measuring range of the low channel OR the low channel must be faulty)
    if ((bms->currentSensorStatusHI == SENSE_GOOD) && ((fabs(currLO) > CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD + (CHANNEL_FILTERING_WIDTH / 2)) || (bms->currentSensorStatusLO != SENSE_GOOD)))
    {
        bms->tractiveSystemCurrent = currHI;
    }
    else if ((bms->currentSensorStatusHI == SENSE_GOOD) && ((fabs(currLO) > CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD - (CHANNEL_FILTERING_WIDTH / 2))))
    {
        float filteredCurrLO = currLO * (currLO - (CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD + (CHANNEL_FILTERING_WIDTH / 2))) / CHANNEL_FILTERING_WIDTH;
        float filteredCUrrHI = currHI * (currLO - (CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD - (CHANNEL_FILTERING_WIDTH / 2))) / CHANNEL_FILTERING_WIDTH;

        bms->tractiveSystemCurrent = filteredCurrLO + filteredCUrrHI;
    }
    else if (bms->currentSensorStatusLO == SENSE_GOOD) // If the above condition is not satisfied, the LO channel must be working in order to use its data
    {
        bms->tractiveSystemCurrent = currLO;
    }
    else // If both sensors are faulty, no current data can be accurately returned
    {
        bms->tractiveSystemCurrent = 0;
        Debug("Failed to read data from current sensor\n");
    }
}
