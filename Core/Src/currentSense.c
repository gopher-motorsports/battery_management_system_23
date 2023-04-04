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
    bms->currentSensorStatusHI = (fabs(currHI) < CURRENT_HIGH_RAIL_THRESHOLD) ? (GOOD) : (BAD);
    bms->currentSensorStatusLO = (fabs(currLO) < CURRENT_LOW_RAIL_THRESHOLD) ? (GOOD) : (BAD);

    // To use the HI current sensor channel, it must be working AND (it must exceed the measuring range of the low channel OR the low channel must be faulty)
    if ((bms->currentSensorStatusHI == GOOD) && ((fabs(currLO) > CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD + (CHANNEL_FILTERING_WIDTH / 2)) || (bms->currentSensorStatusLO != GOOD)))
    {
        bms->tractiveSystemCurrent = currHI;
        bms->tractiveSystemCurrentStatus = GOOD;
    }
    else if ((bms->currentSensorStatusHI == GOOD) && (bms->currentSensorStatusLO == GOOD) && ((fabs(currLO) > CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD - (CHANNEL_FILTERING_WIDTH / 2))))
    {
        float interpolationStart    =   CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD  - (CHANNEL_FILTERING_WIDTH / 2);
        float interpolationRatio    =   (currLO - interpolationStart) / CHANNEL_FILTERING_WIDTH;
        float filteredCurrent       =   ((1.0f - interpolationRatio) * currLO) + (interpolationRatio * currHI);
        bms->tractiveSystemCurrent  =   filteredCurrent;
        bms->tractiveSystemCurrentStatus = GOOD;
    }
    else if (bms->currentSensorStatusLO == GOOD) // If the above condition is not satisfied, the LO channel must be working in order to use its data
    {
        bms->tractiveSystemCurrent = currLO;
        bms->tractiveSystemCurrentStatus = GOOD;
    }
    else // If both sensors are faulty, no current data can be accurately returned
    {
        bms->tractiveSystemCurrentStatus = SENSE_MIA;
        // Debug("Failed to read data from current sensor\n");
    }
}
