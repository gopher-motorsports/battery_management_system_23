#ifndef INC_BMBUTILS_H_
#define INC_BMBUTILS_H_

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_DEPTH 20
#define TABLE_SIZE 33
#define EPSILON 1e-4f

//Generic Lookup Table
typedef struct
{
    uint32_t length;
    float x[TABLE_SIZE];
    float y[TABLE_SIZE];
} LookupTable;

//LookupTable ntcLookup;
//LookupTable zenerLookup;

float lookup(float x, LookupTable* table);
float interpolate(float x, float x1, float x2, float y1, float y2);
uint32_t binarySearch(float *arr, float target, int low, int high, int depth);
bool equals(float f1, float f2);

extern LookupTable ntcLookup;
extern LookupTable zenerLookup;

//Definitions of BMB temperature conversion lookup tables
extern uint8_t tableLength;
extern float ntcVoltArr[33];
extern float zenerVoltArr[33];
extern float tempArr[33];



#endif /* INC_BMBUTILS_H_ */
