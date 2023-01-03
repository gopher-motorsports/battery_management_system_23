#ifndef INC_TIMER_H_
#define INC_TIMER_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t timCount;
    uint32_t timThreshold;
} Timer_S;

void configureTimer(Timer_S* timer, uint32_t timerThreshold);

void clearTimer(Timer_S* timer);

void saturateTimer(Timer_S* timer);

void updateTimer_10ms(Timer_S* timer);

void updateTimer_100ms(Timer_S* timer);

bool checkTimerExpired(Timer_S* timer);


#endif /* INC_TIMER_H_ */
