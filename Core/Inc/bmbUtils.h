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

/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
extern LookupTable_S ntcTable;
extern LookupTable_S zenerTable;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */
// TODO - add description
float lookup(float x, const LookupTable_S* table);

void insertionSort(Brick_S *arr, int numBricks);




#endif /* INC_BMBUTILS_H_ */
