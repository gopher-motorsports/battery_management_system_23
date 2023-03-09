/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "currentSense.h"
#include "GopherCAN.h"
#include <math.h>
#include "debug.h"

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void getTractiveSystemCurrent(Bms_S bms)
{
    // Fetch current sensor data from gophercan
    float currHI = bmsTractiveSystemCurrentHigh_A.data;
    float currLO = bmsTractiveSystemCurrentLow_A.data;

    // If the current exceeds the following threshold in either the positive or negative direction,
    // the sensor input has railed to 0 or 5v and a current sensor error is set
    if(fabs(currHI) < CURRENT_HIGH_RAIL_THRESHOLD)
    {
        bms.currentSensorStatusHI = GOOD;
    }
    else
    {
        Debug("Current sensor high channel has railed!");
        bms.currentSensorStatusHI = MIA;
    }
    
    if(fabs(currLO) < CURRENT_LOW_RAIL_THRESHOLD)
    {
        bms.currentSensorStatusLO = GOOD;
    }
    else
    {
        Debug("Current sensor low channel has railed!");
        bms.currentSensorStatusLO = MIA;
    }

    // To use the HI current sensor channel, it must be working AND (it must exceed the measuring range of the low channel OR the low channel must be faulty)
    if ((bms.currentSensorStatusHI == GOOD) && ((currHI > CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD) || (bms.currentSensorStatusLO != GOOD)))
    {
        bms.tractiveSystemCurrent = currHI;
    }
    else if (bms.currentSensorStatusLO == GOOD) // If the above condition is not satisfied, the LO channel must be working in order to use its data
    {
        bms.tractiveSystemCurrent = currLO;
    }
    else // If both sensors are faulty, no current data can be accurately returned
    {
        Debug("Failed to read data from current sensor");
    }
}

