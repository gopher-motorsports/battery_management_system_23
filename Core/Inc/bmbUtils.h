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

void insertionSort(Brick_S *arr, int numBricks);

// Called when a failed message transaction occurs. Fills leaky bucket slightly
void updateLeakyBucketFail(LeakyBucket_S* bucket);

// Called when a successful message transaction occurs. Drains bucket slightly
void updateLeakyBucketSuccess(LeakyBucket_S* bucket);

// Get the status of the leaky bucket - whether ot not it is considered filled
bool leakyBucketFilled(LeakyBucket_S* bucket);




#endif /* INC_BMBUTILS_H_ */
