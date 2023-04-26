/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <string.h>
#include "bmbUtils.h"
#include "lookupTable.h"
#include "utils.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// The number of values contained in a lookup table
#define TABLE_SIZE 33


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */

const float ntcVoltageArray[TABLE_SIZE] =
			{ 0.830305771, 0.897890409, 0.971038326, 1.049937902, 1.134702713, 1.225347925, 
        1.32176452, 1.423693119, 1.530700113, 1.642159726, 1.757246009, 1.87493832, 
        1.994042452, 2.113227584, 2.231077109, 2.34614984, 2.457047059, 2.562480286, 
        2.661334102, 2.752718092, 2.836002466, 2.910833615, 2.977128647, 3.035051094, 
        3.08497248, 3.127425683, 3.163055933, 3.192574229, 3.216716439, 3.236209893, 
        3.251747985, 3.263972436, 3.273462335};
const float zenerVoltageArray[TABLE_SIZE] =
			{ 1.357903819, 1.367736503, 1.377566917, 1.387395079, 1.397221009, 1.407044725, 
        1.426685585, 1.436502763, 1.456130695, 1.485556757, 1.505163738, 1.534558873, 
        1.563935877, 1.603077481, 1.642187888, 1.681267562, 1.730074517, 1.788581153,
        1.847021367, 1.905396115, 1.963706521, 2.031655975, 2.089832632, 2.147951449, 
        2.206015937, 2.264030521, 2.302681820, 2.350970976, 2.379932725, 2.408886849, 
        2.428185878, 2.447482310, 2.466776486};
const float temperatureArray[TABLE_SIZE] =
			{120, 115, 110, 105, 100, 95, 90, 85, 80, 75, 70, 65, 60, 55, 50, 45, 40, 35,
			 30, 25, 20, 15, 10, 5, 0, -5, -10, -15, -20, -25, -30, -35, -40};


/* ==================================================================== */
/* ======================== GLOBAL VARIABLES ========================== */
/* ==================================================================== */

LookupTable_S ntcTable =  { .length = TABLE_SIZE, .x = ntcVoltageArray, .y = temperatureArray};
LookupTable_S zenerTable= { .length = TABLE_SIZE, .x = zenerVoltageArray, .y = temperatureArray};


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

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
