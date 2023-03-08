/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdbool.h>
#include "main.h"
#include "bms.h"
#include "epaperTask.h"
#include "epaper.h"
#include "cmsis_os.h"

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
extern osMessageQId epaperQueueHandle;

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
        Epaper_Data_S epapData;
        if(xQueueReceive(epaperQueueHandle, &epapData, 0) == pdTRUE)
        {
            epdPopulateData(&epapData);
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