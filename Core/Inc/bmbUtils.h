#ifndef INC_BMBUTILS_H_
#define INC_BMBUTILS_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>


/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

//Generic Lookup Table
typedef struct
{
    const uint32_t length;
    const float* x;			// Pointer to x array
    const float* y;			// Pointer to y array
} LookupTable_S;

typedef struct
{
	int brickIdx;
	float brickV;
} Brick_S;

typedef struct
{
    bool           bucketFilled;      // Whether or not the bucket is filled
	int32_t       fillLevel;         // Current leaky bucket fill level
	const int32_t fillThreshold;     // Leaky bucket fill level triggering alert/behavior
    const int32_t clearThreshold;    // Fill level at which the bucket will no longer be considered filled it if it was before

    const int32_t successDrainCount; // The amount that we will drain (decrement) the leaky bucket on a successful message transaction
    const int32_t failureFillCount;  // The amount that we will fill (increment) the leaky bucket on a failed message transaction
} LeakyBucket_S;

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

// TODO - add description
float lookup(float x, const LookupTable_S* table);

/*!
  @brief   Sorts an array of Brick_S structs by their voltage from lowest to highest
  @param   arr - Pointer to the Brick_S array to be sorted
  @param   numBricks - The length of the array to be sorted
*/
void insertionSort(Brick_S *arr, int numBricks);

/*!
  @brief   Called on a failed trransaction. Partially fills the leaky bucket
  @param   bucket - The leaky bucket struct to update
*/
void updateLeakyBucketFail(LeakyBucket_S* bucket);

/*!
  @brief   Called on a successful transactiion. Partially drains the leaky bucket 
  @param   bucket - The leaky bucket struct to update
*/
void updateLeakyBucketSuccess(LeakyBucket_S* bucket);

/*!
  @brief   Return the status of the leaky bucket
  @param   bucket - The leaky bucket struct to update
  @return  True if the bucket is filled, false otherwise
*/
bool leakyBucketFilled(LeakyBucket_S* bucket);

#endif /* INC_BMBUTILS_H_ */
