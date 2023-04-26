/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "leakyBucket.h"


/* ==================================================================== */
/* ======================== GLOBAL VARIABLES ========================== */
/* ==================================================================== */

// Leaky bucket used to detect ASCI comms failures. Fills when a failed BMB message transaction occurs.
// Decrements on a successful message transaction. When fill level reaches fill threshold bucket filled
// be set to true. When fill level below clear threshold bucketFilled will be set to false.
// This leaky bucket allows for a 1:10 failure to success rate in transactions
LeakyBucket_S asciCommsLeakyBucket = 
              { .bucketFilled = false, .fillLevel = 0, .fillThreshold = 200,
                .clearThreshold = 100, .successDrainCount = 1, .failureFillCount = 10 };


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Called on a failed trransaction. Partially fills the leaky bucket
  @param   bucket - The leaky bucket struct to update
*/
void updateLeakyBucketFail(LeakyBucket_S* bucket)
{
    // Determine how far bucket is from full
    int32_t bucketFillRemaining = bucket->fillThreshold - bucket->fillLevel;
    // Fill bucket with the smaller of - the remaining amount till full or the failureFillCount
    if (bucketFillRemaining <= bucket->failureFillCount)
    {
        bucket->fillLevel += bucketFillRemaining;
        bucket->bucketFilled = true;
    }
    else
    {
        bucket->fillLevel += bucket->failureFillCount;
    }
}

/*!
  @brief   Called on a successful transactiion. Partially drains the leaky bucket 
  @param   bucket - The leaky bucket struct to update
*/
void updateLeakyBucketSuccess(LeakyBucket_S* bucket)
{
    // Drain the smaller of - bucket fill level or successDrainCount
    int32_t drainAmount = (bucket->successDrainCount > bucket->fillLevel) ? bucket->fillLevel : bucket->successDrainCount;
    bucket->fillLevel -= drainAmount;
    if (bucket->fillLevel < bucket->clearThreshold)
    {
        bucket->bucketFilled = false;
    }
}

/*!
  @brief   Return the status of the leaky bucket
  @param   bucket - The leaky bucket struct to update
  @return  True if the bucket is filled, false otherwise
*/
bool leakyBucketFilled(LeakyBucket_S* bucket)
{
    return bucket->bucketFilled;
}

/*!
  @brief   Resets the fill level of the given leaky bucket
  @param   bucket - Pointer to the LeakyBucket_S struct to be reset
  @return  None
*/
void resetLeakyBucket(LeakyBucket_S* bucket)
{
    bucket->fillLevel = 0;
}