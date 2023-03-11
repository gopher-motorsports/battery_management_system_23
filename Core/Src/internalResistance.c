/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "internalResistance.h"
#include "math.h"

/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern Bms_S gBms;

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void getInternalResistance(Bms_S* bms)
{
    static float currentDiscreteBuffer[10];
    static float voltageDiscreteBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][10];
    static uint32_t discreteBufferIndex = 0;

    static float currentAvgBuffer[10] = {[0 ... 9] = -1000.0f};
    static float voltageAvgBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][10];
    static uint32_t avgBufferIndex = 0;

    static uint32_t lastDataUpdate = 0;
    if(HAL_GetTick() - lastDataUpdate > 50)
    {
        lastDataUpdate = HAL_GetTick();

        // Update discrete data buffers with current bms data
        currentDiscreteBuffer[discreteBufferIndex] = (bms->tractiveSystemCurrent / 25.0f);
        for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
        {
            for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
            {
                voltageDiscreteBuffer[i][j][discreteBufferIndex] = bms->bmb[i].brickV[j];
            }    
        }

        if((bms->currentSensorStatusLO == SENSE_GOOD) && (++discreteBufferIndex >= 10))
        {
            discreteBufferIndex = 0;

            // Update avg data buffers with discrete data buffers
            float currentDiscreteSum = 0;
            for(int32_t i = 0; i < 10; i++)
            {
                currentDiscreteSum += currentDiscreteBuffer[i];
            }
            currentAvgBuffer[avgBufferIndex] = currentDiscreteSum / 10;

            for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
            {
                for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
                {
                    float voltageDiscreteSum = 0;
                    for(int32_t k = 0; k < 10; k++)
                    {
                        voltageDiscreteSum += voltageDiscreteBuffer[i][j][k];
                    }
                    voltageAvgBuffer[i][j][avgBufferIndex] = voltageDiscreteSum / 10;
                }    
            }

            if (++avgBufferIndex >= 10)
            {
                avgBufferIndex = 0;
            }

            float maxCurrent = -1000.0f;
            float minCurrent = 1000.0f;
            uint32_t maxCurrentIndex = 0;
            uint32_t minCurrentIndex = 0;

            // Calculate max and min currents in buffer
            for(int32_t i = 0; i < 10; i++)
            {
                if(currentAvgBuffer[i] > -900.0f)
                {
                    if(currentAvgBuffer[i] > maxCurrent)
                    {
                        maxCurrentIndex = i;
                    }
                    else if(currentAvgBuffer[i] < minCurrent)
                    {
                        minCurrentIndex = i;
                    }
                }
            }

            // Calculate greatest dI and dV contained in buffer
            float deltaCurrent = currentAvgBuffer[maxCurrentIndex] - currentAvgBuffer[minCurrentIndex];

            // If delta current threshold is met, internal resistance can be calulated and updated
            if((fabs(deltaCurrent) >= 1) && (fabs(deltaCurrent) <= 75))
            {
                for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
                {
                    for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
                    {
                        float deltaVoltage = voltageAvgBuffer[i][j][maxCurrentIndex] - voltageAvgBuffer[i][j][minCurrentIndex];
                        gBms.brickResistance[i][j] = deltaVoltage / deltaCurrent;
                    }    
                }
            }
        }
    }
}
