/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "internalResistance.h"
#include "math.h"

/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

// Discrete buffers hold consecutive sensor readings from the bms
static float currentDiscreteBuffer[DISCRETE_BUFFER_SIZE];
static float voltageDiscreteBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][DISCRETE_BUFFER_SIZE];

// Circular average buffers hold the average of the discrete buffers, updated each time both discrete buffers fill
// Current buffer is initialized to a placeholder value to indicate data has not been placed in yet
static float currentAvgBuffer[AVERAGE_BUFFER_SIZE] = {[0 ... AVERAGE_BUFFER_SIZE-1] = IR_BAD_DATA};
static float voltageAvgBuffer[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB][AVERAGE_BUFFER_SIZE];

// Buffer pointers hold current index of buffers
static uint32_t discreteBufferIndex = 0;
static uint32_t avgBufferIndex = 0;

// Data good indicates that all sensor readings in discrete buffers are good
static bool currentDataGood = true;
static bool voltageDataGood[NUM_BMBS_PER_PACK][NUM_BRICKS_PER_BMB] = {[0 ... NUM_BMBS_PER_PACK-1] = {[0 ... NUM_BRICKS_PER_BMB-1] = true}};

/* ==================================================================== */
/* ==================== LOCAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

static bool putDiscreteBuffers(Bms_S* bms)
{   
    // If any data in the discrete buffer is from a faulty sensor, set that the current data is bad
    if(bms->currentSensorStatusLO == SENSE_GOOD)
    {
        // Update discrete data buffers with current bms current data
        currentDiscreteBuffer[discreteBufferIndex] = (bms->tractiveSystemCurrent / 25.0f);
    }
    else
    {
        currentDataGood = false;
    }

    // Update discrete data buffers with current bms voltage data
    for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
    {
        for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
        {
            // If any data in the discrete buffer is from a faulty sensor, set that the voltage data is bad
            if(bms->bmb[i].brickVStatus[j] == GOOD)
            {
                voltageDiscreteBuffer[i][j][discreteBufferIndex] = bms->bmb[i].brickV[j];
            }
            else
            {
                voltageDataGood[i][j] = false;
            }
        }    
    }

    // Cycle discrete buffer index
    discreteBufferIndex++;

    // Return true and reset index if discrete buffer filled
    if(discreteBufferIndex >= DISCRETE_BUFFER_SIZE)
    {
        discreteBufferIndex = 0;
        return true;
    }
    return false;
}

static void putAverageBuffers()
{
    /*  An average can only be added to the buffer if the current sensor
        did not return any faulty data to the discrete buffer   */
    if(currentDataGood)
    {
        // Average the discrete buffer and place the output in the average buffer
        float currentDiscreteSum = 0;
        for(int32_t i = 0; i < DISCRETE_BUFFER_SIZE; i++)
        {
            currentDiscreteSum += currentDiscreteBuffer[i];
        }
        currentAvgBuffer[avgBufferIndex] = currentDiscreteSum / DISCRETE_BUFFER_SIZE;

        // Cycle through every BMB and every brick in the accumulator
        for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
        {
            for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
            {
                /*  An average can only be added to the buffer if the current sensor
                    did not return any faulty data to the corresponding discrete buffer   */
                if(voltageDataGood[i][j])
                {
                    // Average the discrete buffer and place the output in the average buffer
                    float voltageDiscreteSum = 0;
                    for(int32_t k = 0; k < DISCRETE_BUFFER_SIZE; k++)
                    {
                        voltageDiscreteSum += voltageDiscreteBuffer[i][j][k];
                    }
                    voltageAvgBuffer[i][j][avgBufferIndex] = voltageDiscreteSum / DISCRETE_BUFFER_SIZE;
                }
                else
                {
                    /*  If bad data was in the corresponding discrete voltage buffer, 
                        it is filled with a placeholder value to indicate it cannot be used for calculation */
                    voltageAvgBuffer[i][j][avgBufferIndex] = IR_BAD_DATA;

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
        currentAvgBuffer[avgBufferIndex] = IR_BAD_DATA;

        // Static data good var is reset for the next discrete buffer filling cycle
        currentDataGood = true;
    }

    // Cycle the circular average buffers and loop if necessary
    avgBufferIndex++;
    if (avgBufferIndex >= AVERAGE_BUFFER_SIZE)
    {
        avgBufferIndex = 0;
    }
}

static void calculateInternalResistance(Bms_S* bms)
{
    // Max and min data initilized to far left and right values
    float maxCurrent = -1000.0f;
    float minCurrent = 1000.0f;

    // Default max and min index is 0
    uint32_t maxCurrentIndex = 0;
    uint32_t minCurrentIndex = 0;

    // Calculate max and min currents in buffer
    for(int32_t i = 0; i < AVERAGE_BUFFER_SIZE; i++)
    {
        // If average current buffer containes placeholder value, IR_BAD_DATA, it is excluded from the max/min calc
        if(!fequals(currentAvgBuffer[i], IR_BAD_DATA))
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
    /*  If avg buffer saturated with IR_BAD_DATA placeholder values, default index 0 will still be set, 
        and delta current will equal 0 (IR_BAD_DATA - IR_BAD_DATA)    */
    float deltaCurrent = currentAvgBuffer[maxCurrentIndex] - currentAvgBuffer[minCurrentIndex];

    // If delta current threshold is met, internal resistance can be calulated and updated
    // This threshold should always exceed 0, so if avg buffer is saturated with IR_BAD_DATA, operation will be skipped
    if(fabs(deltaCurrent) >= IR_CALC_MIN_CURRENT_DELTA)
    {
        // Cycle through every BMB and every brick in the accumulator
        for(int32_t i = 0; i < NUM_BMBS_PER_PACK; i++)
        {
            for(int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
            {
                // If average current buffer containes placeholder value, IR_BAD_DATA, it is excluded from this calculation cycle
                if((!fequals(voltageAvgBuffer[i][j][maxCurrentIndex], IR_BAD_DATA)) && (!fequals(voltageAvgBuffer[i][j][minCurrentIndex], IR_BAD_DATA)))
                {
                    float deltaVoltage = voltageAvgBuffer[i][j][maxCurrentIndex] - voltageAvgBuffer[i][j][minCurrentIndex];
                    bms->bmb[i].brickResistance[j] = deltaVoltage / deltaCurrent;
                }
            }    
        }
    }
}

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void updateInternalResistanceCalcs(Bms_S* bms)
{
    /*  Place current and voltage data in discrete buffers.
        discreteBuffersFull is set to true if the discrete buffer is full   */
    bool discreteBuffersFull = putDiscreteBuffers(bms);

    /*  Once discrete buffers full, add a data point to the circular average buffer
        and attempt to perform a IR calculation  */
    if(discreteBuffersFull)
    {
        putAverageBuffers();
        calculateInternalResistance(bms);
    }
}
