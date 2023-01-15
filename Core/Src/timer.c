/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "main.h"
#include "timer.h"


void updateTimer(Timer_S* timer)
{
    uint32_t timeTilExpiration = timer->timThreshold - timer->timCount;
    uint32_t timeSinceLastUpdate = HAL_GetTick() - timer->lastUpdate;
    timer->lastUpdate = HAL_GetTick();
    if (timeTilExpiration < timeSinceLastUpdate)
    {
        timer->timCount += timeTilExpiration;
    }
    else
    {
        timer->timCount += timeSinceLastUpdate;
    }
}

void configureTimer(Timer_S* timer, uint32_t timerThreshold)
{
    timer->timCount = 0;
    timer->lastUpdate = HAL_GetTick();
    timer->timThreshold = timerThreshold;
}

void clearTimer(Timer_S* timer)
{
    timer->timCount = 0;
}

void saturateTimer(Timer_S* timer)
{
    timer->timCount = timer->timThreshold;
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