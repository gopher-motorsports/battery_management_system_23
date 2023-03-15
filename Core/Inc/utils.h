#ifndef UTILS_H_
#define UTILS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdbool.h>
#include <math.h>
#include "main.h"
#include "debug.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The max spacing between two floats before they are considered not equal
#define EPSILON 1e-4f

/* ==================================================================== */
/* ============================== MACROS ============================== */
/* ==================================================================== */

/*!
  @brief   Determine if two floating point values are equal
  @returns True if equal, false if not equal
*/
#define fequals(a, b) (fabs(a - b) < EPSILON) ? (true) : (false)

// Macro that retries a SPI TX/RX if it fails
#define SPIRTRY(fn, hspi, ...) \
	for (int i = 0; i < 2; i++) \
	{ \
		HAL_StatusTypeDef status = fn(hspi, __VA_ARGS__); \
		if (status == HAL_OK) { break; } \
		Debug("Failed ASCI SPI transmission - Retrying!\n"); \
		HAL_SPI_Abort(hspi); \
	}

#endif /* UTILS_H_ */
