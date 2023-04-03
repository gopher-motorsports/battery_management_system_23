/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <string.h>
#include "bmbUtils.h"
#include "utils.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The max number of calls that are allowed in a binary search instance before erroring
#define MAX_DEPTH 20
// The number of values contained in a lookup table
#define TABLE_SIZE 33


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

const uint8_t tableLength = 33;
const float ntcVoltageArray[33] =
			{0.830305771, 0.897890409, 0.971038326, 1.049937902, 1.134702713, 1.225347925, 
             1.32176452, 1.423693119, 1.530700113, 1.642159726, 1.757246009, 1.87493832, 
             1.994042452, 2.113227584, 2.231077109, 2.34614984, 2.457047059, 2.562480286, 
             2.661334102, 2.752718092, 2.836002466, 2.910833615, 2.977128647, 3.035051094, 
             3.08497248, 3.127425683, 3.163055933, 3.192574229, 3.216716439, 3.236209893, 
             3.251747985, 3.263972436, 3.273462335};
const float zenerVoltageArray[33] =
			{1.357903819, 1.367736503, 1.377566917, 1.387395079, 1.397221009, 1.407044725, 
             1.426685585, 1.436502763, 1.456130695, 1.485556757, 1.505163738, 1.534558873, 
             1.563935877, 1.603077481, 1.642187888, 1.681267562, 1.730074517, 1.788581153,
             1.847021367, 1.905396115, 1.963706521, 2.031655975, 2.089832632, 2.147951449, 
             2.206015937, 2.264030521, 2.302681820, 2.350970976, 2.379932725, 2.408886849, 
             2.428185878, 2.447482310, 2.466776486};
const float temperatureArray[33] =
			{120, 115, 110, 105, 100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35,
			 30, 25, 20, 15, 10, 5, 0, -5, -10, -15, -20, -25, -30, -35, -40};


/* ==================================================================== */
/* ======================== GLOBAL VARIABLES ========================== */
/* ==================================================================== */

LookupTable_S ntcTable =  { .length = tableLength, .x = ntcVoltageArray, .y = temperatureArray};
LookupTable_S zenerTable= { .length = tableLength, .x = zenerVoltageArray, .y = temperatureArray};

// Leaky bucket used to detect ASCI comms failures. Fills when a failed BMB message transaction occurs.
// Decrements on a successful message transaction. When fill level reaches fill threshold bucket filled
// be set to true. When fill level below clear threshold bucketFilled will be set to false.
// This leaky bucket allows for a 1:10 failure to success rate in transactions
LeakyBucket_S asciCommsLeakyBucket = 
              { .bucketFilled = false, .fillLevel = 0, .fillThreshold = 200,
                .clearThreshold = 100, .successDrainCount = 1, .failureFillCount = 10 };


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/*!
    @brief   Search in an array of floats for a specific value using binary search
    @param   arr - The array of floats to search
    @param   target - The target value to search for
    @param   low - The lowest index in the search range
    @param   high - The highest index in the search range
    @return  The index of the upper float that bounds the value of interest
*/
static uint32_t binarySearch(const float *arr, const float target, int low, int high)
{
    //check for invalid bounds
    if(low > high)
    {
        return 0;
    }

    //clamp target to bounds
    if(target < arr[low])
    {
        return low;
    }
    else if(target > arr[high])
    {
        return high;
    }

    uint8_t depth = 0;
    while((low <= high) && (depth < MAX_DEPTH))
    {
        //update middle value
        int mid = (high - low) / 2 + low;

        //target is below index m
        if(target <= arr[mid+1])
        {
            //target is between index m and m-1, return m
            if(target >= arr[mid])
            {
                return mid;
            }
            //target is less than index m, update bounds
            else
            {
                high = mid;
            }
        }
        //target is greater than index m, update bounds
        else
        {
            low = mid + 1;
        }
        depth++;
    }
    return 0;
}

/*!
    @brief   Interpolate a value y given two endpoints (x1, y1) and (x2, y2) and an input value x
    @param   x - The input value to interpolate
    @param   x1 - The x-coordinate of the first endpoint
    @param   x2 - The x-coordinate of the second endpoint
    @param   y1 - The y-coordinate of the first endpoint
    @param   y2 - The y-coordinate of the second endpoint
    @return  The interpolated value of y at x
*/
static float interpolate(float x, float x1, float x2, float y1, float y2)
{
    //if infinite slope, return y2
    if (fequals(x1, x2))
    {
        return y2;
    }
    else
    {
        // map input x to y axis with slope m = (y2-y1)/(x2-x1)
        return (x - x1) * ((y2-y1) / (x2-x1)) + y1;
    }
}

/*!
  @brief   Search a sorted array of Brick_S structs using binary search
  @param   arr - Pointer to the Brick_S array to be searched
  @param   l - Index of the left element of the array 
  @param   r - Index of the right element of the array
  @param   v - The target voltage to be compared to the Brick_S struct voltage
  @return  Index at which voltage is to be inserted
*/
int32_t brickBinarySearch(Brick_S *arr, int l, int r, float v)
{
  while (l <= r)
  {
    int32_t m = l + (r - l) / 2;
    if (fequals(arr[m].brickV, v))
    {
    	return m;
    }
    if (arr[m].brickV < v)
    {
    	l = m + 1;
    }
    else
    {
    	r = m - 1;
    }
  }
  return l;
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */
/*!
    @brief   Look up the value of a function in a lookup table, given an input value
    @param   x - The input value to look up in the table
    @param   table - A pointer to the lookup table containing the function values
    @return  The interpolated value of the function corresponding to the input value, or 0 if an error occurs
*/
float lookup(float x, const LookupTable_S* table)
{
    //Clamp input to bounds of lookup table
    if(x < table->x[0])
    {
        return table->y[0];
    }
    else if(x > table->x[table->length-1])
    {
        return table->y[table->length-1];
    }

    //Search for index of lookup table voltage that is closest to and less than input voltage
    uint32_t i = binarySearch(table->x, x, 0, table->length-1);

    //Return 0 if binary search error
    if(i == -1)
    {
        return 0;
    }

    //Interpolate temperature from lookup table
    return interpolate(x, table->x[i], table->x[i+1], table->y[i], table->y[i+1]);
}

/*!
  @brief   Sorts an array of Brick_S structs by their voltage from lowest to highest
  @param   arr - Pointer to the Brick_S array to be sorted
  @param   numBricks - The length of the array to be sorted
*/
void insertionSort(Brick_S *arr, int numBricks)
{
  for (int32_t unsortedIdx = 1; unsortedIdx < numBricks; unsortedIdx++)
  {
    Brick_S temp = arr[unsortedIdx];
    int32_t sortedIdx = unsortedIdx - 1;
    int32_t pos = brickBinarySearch(arr, 0, sortedIdx, temp.brickV);
    while (sortedIdx >= pos)
    {
      arr[sortedIdx + 1] = arr[sortedIdx];
      sortedIdx--;
    }
    arr[sortedIdx + 1] = temp;
  }
}

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
