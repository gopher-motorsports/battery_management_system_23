/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "timer.h"


void updateTimer(Timer_S* timer, uint32_t timeStep_MS);

void updateTimer(Timer_S* timer, uint32_t timeStep_MS)
{
    uint32_t timDifference = timer->timThreshold - timer->timCount;
    if (timDifference < timeStep_MS)
    {
        timer->timCount += timDifference;
    }
    else
    {
        timer->timCount += timeStep_MS;
    }
}


void configureTimer(Timer_S* timer, uint32_t timerThreshold)
{
    timer->timCount = 0;
    timer->timThreshold = timerThreshold;
}

void clearTimer(Timer_S* timer)
{
    timer->timCount = 0;
}

void updateTimer_10ms(Timer_S* timer)
{
    uint32_t timeStep = 10;
    updateTimer(timer, timeStep);
}

void updateTimer_100ms(Timer_S* timer)
{
    uint32_t timeStep = 100;
    updateTimer(timer, timeStep);
}

bool checkTimerExpired(Timer_S* timer)
{
    if (timer->timCount >= timer->timThreshold)
    {
        return true;
    }
    else
    {
        return false;
    }
}