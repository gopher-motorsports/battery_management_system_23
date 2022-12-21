/*
 * batteryPack.h
 *
 *  Created on: Aug 14, 2022
 *      Author: sebas
 */

#ifndef INC_BMB_H_
#define INC_BMB_H_

#include <stdio.h>
#include <stdbool.h>

#define DEVCFG1		0x10
#define GPIO 		0x11
#define MEASUREEN 	0x12
#define SCANCTRL	0x13
#define ACQCFG		0x19
#define CELLn		0x20
#define VBLOCK		0x2C
#define AIN1		0x2D
#define AIN2		0x2E

#define MUX1		0x00
#define MUX2		0x01
#define MUX3		0x02
#define MUX4		0x03
#define MUX5		0x04
#define MUX6		0x05
#define MUX7		0x06
#define MUX8		0x07

#define NUM_BRICKS_PER_BMB		12
#define NUM_BOARD_TEMP_PER_BMB 	4
#define NUM_BMBS_PER_PACK		1

// 3.3V range & 12 bit reading - 3.3/(2^12) = 805.664 uV/bit
#define CONVERT_12BIT_TO_3V3	0.000805664f;
// 5V range & 14 bit reading   - 5/(2^14)   = 305.176 uV/bit
#define CONVERT_14BIT_TO_5V		0.000305176f
// 60V range & 14 bit ADC 	   - 60/(2^14)  = 3.6621 mV/bit
#define CONVERT_14BIT_TO_60V	0.0036621f

typedef enum
{
	SNA = 0,	// Value on startup
	GOOD,		// Data nominal
	BAD,		// Data was acquired but isn't trustworthy
	MIA			// Data wasn't aquired
} Bmb_Sensor_Status_E;

typedef struct
{
	uint32_t numBricks;
	// TODO - set initial status value to SNA
	Bmb_Sensor_Status_E brickVStatus[NUM_BRICKS_PER_BMB];
	float brickV[NUM_BRICKS_PER_BMB];
	float stackV;
	float blockV;

	Bmb_Sensor_Status_E brickTempStatus[NUM_BRICKS_PER_BMB];
	float brickTempVoltage[NUM_BRICKS_PER_BMB];
	float brickTemp[NUM_BRICKS_PER_BMB];

	Bmb_Sensor_Status_E boardTempStatus[NUM_BRICKS_PER_BMB];
	float boardTempVoltage[NUM_BOARD_TEMP_PER_BMB];
	float boardTemp[NUM_BOARD_TEMP_PER_BMB];

	float maxBrickV;
	float minBrickV;
	float avgBrickV;

	float maxBrickTemp;
	float minBrickTemp;
	float avgBrickTemp;

	float maxBoardTemp;
	float minBoardTemp;
	float avgBoardTemp;

	float aux1;
	float aux2;
	// add gpio mode

} Bmb_S;

typedef struct
{
	uint32_t numBmbs;
	Bmb_S bmb[NUM_BMBS_PER_PACK];
} Pack_S;


bool initASCI(uint32_t *numBmbs);

void initBmbs(uint32_t numBmbs);
void setGpio(uint32_t numBmbs, uint16_t gpioSetting);
void readBoardTemps(uint32_t numBmbs);
void updateBmbData(uint32_t numBmbs);
void aggregateBrickVoltages(uint32_t numBmbs);


#endif /* INC_BMB_H_ */
