#ifndef UTILS_H_
#define UTILS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdbool.h>
#include <math.h>
#include "main.h"
#include "debug.h"
#include "cmsis_os.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The max spacing between two floats before they are considered not equal
#define EPSILON 1e-4f

// Max unsigned long int value
#define ULONG_MAX 0xffffffffUL

// Number of SPI retry events
#define NUM_SPI_RETRY 3

/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum 
{
    SPI_TIMEOUT = 0,
    SPI_SUCCESS,
    SPI_ERROR
} SPI_STATUS_E;

/* ==================================================================== */
/* ============================== MACROS ============================== */
/* ==================================================================== */

/*!
  @brief   Determine if two floating point values are equal
  @returns True if equal, false if not equal
*/
#define fequals(a, b) (fabs(a - b) < EPSILON) ? (true) : (false)

/*!
  @brief    Carries out a HAL SPI command and blocks the executing task until SPI complete
  @param    fn          HAL SPI function to execute
  @param    hspi        SPI bus handle for transaction
  @param    timeout     Max duration of task blocking state
  @param    status      U32 status variable that will be set according to the SPI transaction state on completion
  @param    callingFunc Name of the function that called SPI_TRANSMIT for debugging purposes
*/
#define SPI_TRANSMIT(fn, hspi, timeout, status, callingFunc, ...) \
    for (uint32_t i = 0; i < NUM_SPI_RETRY; i++) \
    { \
        if (fn(hspi, __VA_ARGS__) != HAL_OK) \
        { \
            Debug("SPI transmission failed to start in function: %s! - Attempt %lu!\n", #callingFunc, i+1); \
            HAL_SPI_Abort(hspi); \
            continue; \
        } \
        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, status, timeout) != pdPASS) \
        { \
            Debug("SPI transmission TIMED OUT in %s! - Aborting!\n", #callingFunc); \
            break; \
        } \
		if (status != SPI_SUCCESS) \
		{ \
			Debug("SPI transmission error occured in function: %s! - Attempt %lu!\n", #callingFunc, i+1); \
			continue; \
		} \
        break; \
    }

/*!
  @brief    Blocks the current task and waits for an external interupt to be recieved
  @param    timeout     Max duration of task blocking state
  @param    callingFunc Name of the function that called SPI_TRANSMIT for debugging purposes
*/
#define WAIT_EXT_INT(timeout, callingFunc) \
	uint32_t notificationValue = 0; \
	if (xTaskNotifyWait(pdFALSE, ULONG_MAX, &notificationValue, timeout) != pdPASS) \
	{ \
		Debug("External interrupt failed to occur in %s!\n", #callingFunc); \
	} \

#endif /* UTILS_H_ */
