#ifndef INC_LEAKYBUCKET_H_
#define INC_LEAKYBUCKET_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdint.h>
#include <stdbool.h>

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct
{
    bool          bucketFilled;      // Whether or not the bucket is filled
    int32_t       fillLevel;         // Current leaky bucket fill level
    const int32_t fillThreshold;     // Leaky bucket fill level triggering alert/behavior
    const int32_t clearThreshold;    // Fill level at which the bucket will no longer be considered filled it if it was before

    const int32_t successDrainCount; // The amount that we will drain (decrement) the leaky bucket on a successful message transaction
    const int32_t failureFillCount;  // The amount that we will fill (increment) the leaky bucket on a failed message transaction
} LeakyBucket_S;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

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

/*!
  @brief   Resets the fill level of the given leaky bucket
  @param   bucket - Pointer to the LeakyBucket_S struct to be reset
  @return  None
*/
void resetLeakyBucket(LeakyBucket_S* bucket);

#endif /* INC_LEAKYBUCKET_H_ */
