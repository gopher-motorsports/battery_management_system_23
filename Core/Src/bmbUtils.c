/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <string.h>
#include "bmbUtils.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define EPSILON 1e-4f
#define MAX_DEPTH 20
#define TABLE_SIZE 33

/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

const uint8_t tableLength = 33;
const float ntcVoltageArray[33] =
			{0.6994342978, 0.7598430424, 0.8258348331, 0.8977261194, 0.9757846124, 1.060206506,
			 1.151089638, 1.248403196, 1.351955519, 1.461362786, 1.576022682, 1.695098099,
			 1.817516016, 1.941985476, 2.067036237, 2.191076693, 2.31246698, 2.429601265,
			 2.540992049, 2.645348688, 2.741642152, 2.82914902, 2.907470039, 2.97652236,
			 3.036508572, 3.087868798, 3.131223525, 3.167314536, 3.196949769, 3.220955937,
			 3.240140792, 3.255265383, 3.267025592};
const float zenerVoltageArray[33] =
			{1.34666047, 1.35652775, 1.366393171, 1.376256748, 1.386118496, 1.395978431,
			 1.415692919, 1.4255475, 1.445251403, 1.474794297, 1.494481055, 1.523998648,
			 1.553501416, 1.592815768, 1.632104629, 1.671368385, 1.720413256, 1.779216693,
			 1.837965855, 1.89666148, 1.955304426, 2.023656124, 2.082189285, 2.140674807,
			 2.199115467, 2.25751479, 2.296426822, 2.345046035, 2.374207727, 2.403363004,
			 2.422796712, 2.442228214, 2.461657789};
const float temperatureArray[33] =
			{120, 115, 110, 105, 100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35,
			 30, 25, 20, 15, 10, 5, 0, -5, -10, -15, -20, -25, -30, -35, -40};


/* ==================================================================== */
/* ======================== GLOBAL VARIABLES ========================== */
/* ==================================================================== */

LookupTable_S ntcTable = {tableLength, ntcVoltageArray, temperatureArray};
LookupTable_S zenerTable= {tableLength, zenerVoltageArray, temperatureArray};


/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

bool equals(float f1, float f2);
uint32_t binarySearch(const float *arr, const float target, int low, int high);
float interpolate(float x, float x1, float x2, float y1, float y2);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

bool equals(float f1, float f2)
{
    if (fabs(f1 - f2) < EPSILON)
    {
        return true;
    }
    return false;
}

uint32_t binarySearch(const float *arr, const float target, int low, int high)
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

float interpolate(float x, float x1, float x2, float y1, float y2)
{
    //if infinite slope, return y2
    if (equals(x1, x2))
    {
        return y2;
    }
    else
    {
        // map input x to y axis with slope m = (y2-y1)/(x2-x1)
        return (x - x1) * ((y2-y1) / (x2-x1)) + y1;
    }
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

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

// Binary search function for inserting element in sorted array
int brickBinarySearch(Brick_S *arr, int l, int r, float v)
{
  while (l <= r)
  {
    int m = l + (r - l) / 2;
    if (equals(arr[m].brickV, v))
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

// Insertion sort function for sorting array of structs by float value
void insertionSort(Brick_S *arr, int numBricks)
{
  for (int unsortedIdx = 1; unsortedIdx < numBricks; unsortedIdx++)
  {
    Brick_S temp = arr[unsortedIdx];
    int sortedIdx = unsortedIdx - 1;
    int pos = brickBinarySearch(arr, 0, sortedIdx, temp.brickV);
    while (sortedIdx >= pos)
    {
      arr[sortedIdx + 1] = arr[sortedIdx];
      sortedIdx--;
    }
    arr[sortedIdx + 1] = temp;
  }
}
