#ifndef INC_SOC_H_
#define INC_SOC_H_

#include "timer.h"

typedef struct
{
    uint32_t initialMilliCoulombCount;
    int32_t accumulatedMilliCoulombs;
} CoulombCounter_S;

typedef struct
{
    // Inputs
    float minCellVoltage;
    float curAccumulatorCurrent;

    // Outputs
    bool socByOcvGood;
    CoulombCounter_S coulombCounter;
    Timer_S socByOcvGoodTimer;
    float lastGoodSoc;
    float socByOcv;
    float socByCoulombCounting;
    float soeByOcv;
    float soeByCoulombCounting;
} Soc_S;




float getSocFromCellVoltage(float cellVoltage);

float getSoeFromSoc(float soc);

void updateSocAndSoe(float minCellVoltage, float curAccumulatorCurrent, Soc_S* soc);


#endif /* INC_SOC_H_ */
