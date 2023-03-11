/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "internalResistance.h"
#include "math.h"

/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

// Discrete buffers hold consecutive sensor readings from the bms
static float currentDiscreteBuffer[DISCRETE_CURRENT_BUFFER_SIZE];
static float voltageDiscreteBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][DISCRETE_VOLTAGE_BUFFER_SIZE];

// Discrete buffer pointers hold current index of discrete buffers
static uint32_t discreteCurrentBufferIndex = 0;
static uint32_t discreteVoltageBufferIndex = 0;

// Data good indicates that all sensor readings in discrete buffers are good
static bool currentDataGood = true;
static bool voltageDataGood[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB] = {true};

// Data ready indicates that the discrete buffer has filled
static bool currentDataReady = false;
static bool voltageDataReady = false;

/* ==================================================================== */
/* ==================== LOCAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

static void calculateInternalResistance(Bms_S* bms)
{
    // Circular average buffers hold the average of the discrete buffers, updated each time both discrete buffers fill
    // Current buffer is initialized to a placeholder value to indicate data has not been placed in yet
    static float currentAvgBuffer[AVERAGE_BUFFER_SIZE] = {[0 ... AVERAGE_BUFFER_SIZE-1] = -1000.0f};
    static float voltageAvgBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][AVERAGE_BUFFER_SIZE];
    static uint32_t avgBufferIndex = 0;

    /*  An average can only be added to the buffer if the current sensor
        did not return any faulty data to the discrete buffer   */
    if(currentDataGood)
    {
        // Average the discrete buffer and place the output in the average buffer
        float currentDiscreteSum = 0;
        for(int32_t i = 0; i < DISCRETE_CURRENT_BUFFER_SIZE; i++)
        {
            currentDiscreteSum += currentDiscreteBuffer[i];
        }
        currentAvgBuffer[avgBufferIndex] = currentDiscreteSum / DISCRETE_CURRENT_BUFFER_SIZE;

        // Cycle through every BMB and every brick in the accumulator
        for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
        {
            for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
            {
                /*  An average can only be added to the buffer if the current sensor
                    did not return any faulty data to the corresponding discrete buffer   */
                if(voltageDataGood)
                {
                    // Average the discrete buffer and place the output in the average buffer
                    float voltageDiscreteSum = 0;
                    for(int32_t k = 0; k < DISCRETE_VOLTAGE_BUFFER_SIZE; k++)
                    {
                        voltageDiscreteSum += voltageDiscreteBuffer[i][j][k];
                    }
                    voltageAvgBuffer[i][j][avgBufferIndex] = voltageDiscreteSum / DISCRETE_VOLTAGE_BUFFER_SIZE;
                }
                else
                {
                    /*  If bad data was in the corresponding discrete voltage buffer, 
                        it is filled with a placeholder value to indicate it cannot be used for calculation */
                    voltageAvgBuffer[i][j][avgBufferIndex] = -1000.0f;

                    // Static data good var is reset for the next discrete buffer filling cycle
                    voltageDataGood[i][j] = true;
                }
            }    
        }
    }
    else
    {
        /*  If bad data was in the corresponding discrete voltage buffer, 
            it is filled with a placeholder value to indicate it cannot be used for calculation */
        currentAvgBuffer[avgBufferIndex] = -1000.0f;

        // Static data good var is reset for the next discrete buffer filling cycle
        currentDataGood = true;
    }

    // Cycle the circular average buffers and loop if necessary
    if (++avgBufferIndex >= AVERAGE_BUFFER_SIZE)
    {
        avgBufferIndex = 0;
    }

    // Max and min data initilized to far left and right values
    float maxCurrent = -1000.0f;
    float minCurrent = 1000.0f;

    // Default max and min index is 0
    uint32_t maxCurrentIndex = 0;
    uint32_t minCurrentIndex = 0;

    // Calculate max and min currents in buffer
    for(int32_t i = 0; i < AVERAGE_BUFFER_SIZE; i++)
    {
        // If average current buffer containes placeholder value, -1000.0f, it is excluded from the max/min calc
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

    //  Calculate greatest dI and dV contained in buffer
    /*  If avg buffer saturated with -1000.0f placeholder values, default index 0 will still be set, 
        and delta current will equal 0 (-1000.0f - -1000.0f)    */
    float deltaCurrent = currentAvgBuffer[maxCurrentIndex] - currentAvgBuffer[minCurrentIndex];

    // If delta current threshold is met, internal resistance can be calulated and updated
    // This threshold should always exceed 0, so if avg buffer is saturated with -1000.0f, operation will be skipped
    if((fabs(deltaCurrent) >= IR_CALC_MIN_CURRENT_DELTA) && (fabs(deltaCurrent) <= CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD))
    {
        // Cycle through every BMB and every brick in the accumulator
        for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
        {
            for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
            {
                // If average current buffer containes placeholder value, -1000.0f, it is excluded from this calculation cycle
                if((voltageAvgBuffer[i][j][maxCurrentIndex] > -900.0f) && (voltageAvgBuffer[i][j][minCurrentIndex] > -900.0f))
                {
                    float deltaVoltage = voltageAvgBuffer[i][j][maxCurrentIndex] - voltageAvgBuffer[i][j][minCurrentIndex];
                    bms->brickResistance[i][j] = deltaVoltage / deltaCurrent;
                }
            }    
        }
    }

    // Static data ready vars set to 0 for next discrete buffer filling cycle 
    currentDataReady = false;
    voltageDataReady = false;

    // Reset discrete buffer pointers so data begins filling from index 0 
    discreteCurrentBufferIndex = 0;
    discreteVoltageBufferIndex = 0;
}

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void putCurrentBuffer(Bms_S* bms)
{   
    // If any data in the discrete buffer is from a faulty sensor, set that the current data is bad
    if(bms->currentSensorStatusLO == SENSE_GOOD)
    {
        // Update discrete data buffers with current bms current data
        currentDiscreteBuffer[discreteCurrentBufferIndex] = (bms->tractiveSystemCurrent / 25.0f);
    }
    else
    {
        currentDataGood = false;
    }

    // Cycle buffer and wrap to 0 if needed
    if(++discreteCurrentBufferIndex >= DISCRETE_CURRENT_BUFFER_SIZE)
    {
        discreteCurrentBufferIndex = 0;

        // Indicate that the discrete current buffer has been filled
        currentDataReady = true;

        // Perform a IR calculation if the voltage data is also ready
        if(voltageDataReady)
        {
            calculateInternalResistance(bms);
        }
    }
}

void putVoltageBuffer(Bms_S* bms)
{
    // Update discrete data buffers with current bms voltage data
    for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
    {
        for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
        {
            // If any data in the discrete buffer is from a faulty sensor, set that the voltage data is bad
            if(bms->bmb[i].brickVStatus[j] == GOOD)
            {
                voltageDiscreteBuffer[i][j][discreteVoltageBufferIndex] = bms->bmb[i].brickV[j];
            }
            else
            {
                voltageDataGood[i][j] = false;
            }
        }    
    }
    
    // Cycle buffer and wrap to 0 if needed
    if(++discreteVoltageBufferIndex >= DISCRETE_VOLTAGE_BUFFER_SIZE)
    {
        discreteVoltageBufferIndex = 0;

        // Indicate that the discrete voltage buffers have been filled
        voltageDataReady = true;

        // Perform a IR calculation if the current data is also ready
        if(currentDataReady)
        {
            calculateInternalResistance(bms);
        }
    }
}
