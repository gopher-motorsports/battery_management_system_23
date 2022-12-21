#ifndef INC_BMBUTILS_H_
#define INC_BMBUTILS_H_

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_DEPTH 20
#define TABLE_SIZE 33

//Generic Lookup Table
typedef struct
{
    const uint32_t length;
    const float* x;			// Pointer to x array
    const float* y;			// Pointer to y array
} LookupTable;


void initLookup();

float lookup(float x, const LookupTable* table);

extern LookupTable ntcTable;
extern LookupTable zenerTable;


#endif /* INC_BMBUTILS_H_ */
