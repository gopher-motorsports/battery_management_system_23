/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdbool.h>
#include "main.h"
#include "bms.h"
#include "epaperTask.h"
#include "epaper.h"

/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

bool init = true;
uint32_t lastDataUpdate = 0;
uint32_t numUpdates = 0;

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern Bms_S gBms;

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void runEpaper()
{
    if(init)
    {
        // Initialize the epaper display and perform a full refresh
        epdInit();
        epdFullRefresh();
        
        init = false;
    }
    else
    {
        // Perform a partial refresh at a given frequency
        if((HAL_GetTick() - lastDataUpdate) > DATA_REFESH_PERIOD_MS)
        {
            lastDataUpdate = HAL_GetTick();

            // Perform a partial update with current BMS data
            epdPopulateData(&gBms);
            numUpdates++;

            // Perform a full update after a given number of partial refeshes
            if(numUpdates >= NUM_PARTIAL_REFRESH_BEFORE_FULL_UPDATE)
            {
                epdFullRefresh();
                numUpdates = 0;
            }
        }
    }
}