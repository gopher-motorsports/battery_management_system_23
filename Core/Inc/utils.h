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

// Varaibles used for task notification flags
#define TASK_NO_OP          0UL
#define TASK_CLEAR_FLAGS    0xffffffffUL

// Number of SPI retry events
#define NUM_SPI_RETRY 3

/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum 
{
    INTERRUPT_TIMEOUT = 0,  // Interrupt timed out
    INTERRUPT_SUCCESS,      // Interrupt was successful
    INTERRUPT_ERROR         // Interrupt error occured
} INTERRUPT_STATUS_E;       // Interupt status enum for task notification flags

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
  @param    callingFunc Name of the function that called SPI_TRANSMIT for debugging purposes
  @return   Returns a SPI_STATUS_E corresponding to the resulting state of the transaction
*/
#define SPI_TRANSMIT(fn, hspi, timeout, callingFunc, ...) \
  ({ \
    /* Notification value used for task notification */ \
    uint32_t spiStatus = 0; \
    /* SPI transaction will not be attempted more than NUM_SPI_RETRY */ \
    for (uint32_t i = 0; i < NUM_SPI_RETRY; i++) \
    { \
      /* Attempt to start SPI transaction */ \
      if (fn(hspi, __VA_ARGS__) != HAL_OK) \
      { \
        /* If SPI fails to start, HAL must abort transaction. SPI retries */ \
        Debug("SPI transmission failed to start in function: %s! - Attempt %lu!\n", #callingFunc, i+1); \
        HAL_SPI_Abort(hspi); \
        continue; \
      } \
      /* Wait for SPI interrupt to occur. SpiStatus will hold notification value indicating status of transaction */ \
      if (xTaskNotifyWait(TASK_NO_OP, TASK_CLEAR_FLAGS, &spiStatus, timeout) != pdTRUE) \
      { \
        /* If no SPI interrupt occurs in time, transaction is aborted to prevent any longer delay */\
        Debug("SPI transmission TIMED OUT in %s! - Aborting!\n", #callingFunc); \
        break; \
      } \
      /* If SPI SUCCESS bit is not set in notification value, SPI error has occured. SPI retries */ \
      if (!(spiStatus & INTERRUPT_SUCCESS)) \
      { \
        Debug("SPI transmission error occured in function: %s! - Attempt %lu!\n", #callingFunc, i+1); \
        continue; \
      } \
      /* Break on successful transaction */ \
      break; \
    } \
    /* Macro will evaluate to this value - can be used as a return value to handle SPI failures further */ \
    (INTERRUPT_STATUS_E) spiStatus; \
  })

/*!
  @brief    Blocks the current task and waits for an external interupt to be recieved
  @param    timeout     Max duration of task blocking state
  @param    callingFunc Name of the function that called SPI_TRANSMIT for debugging purposes
*/
#define WAIT_EXT_INT(timeout, callingFunc) \
  ({ \
    /* Notification value used for task notification */ \
    uint32_t extIntStatus = 0; \
    /* Block calling task and wait for external interrupt to occur */ \
    if (xTaskNotifyWait(TASK_NO_OP, TASK_CLEAR_FLAGS, &extIntStatus, timeout) != pdTRUE) \
    { /* External interrupt has failed to occur */ \
      Debug("External interrupt failed to occur in %s!\n", #callingFunc); \
    } \
    /* Macro will evaluate to this value - can be used as a return value to handle EXTI failures further */ \
    (INTERRUPT_STATUS_E) extIntStatus; \
  })

#endif /* UTILS_H_ */
