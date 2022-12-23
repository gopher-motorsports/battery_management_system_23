#ifndef INC_BMB_H_
#define INC_BMB_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include <stdio.h>
#include <stdbool.h>


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define DEVCFG1		0x10
#define GPIO 		0x11
#define MEASUREEN 	0x12
#define SCANCTRL	0x13
#define ACQCFG		0x19
#define CELLn		0x20
#define VBLOCK		0x2C
#define AIN1		0x2D
#define AIN2		0x2E

#define NUM_BRICKS_PER_BMB		12
#define NUM_BOARD_TEMP_PER_BMB 	4
#define NUM_BMBS_PER_PACK		1

// 3.3V range & 12 bit reading - 3.3/(2^12) = 805.664 uV/bit
#define CONVERT_12BIT_TO_3V3	0.000805664f;
// 5V range & 14 bit reading   - 5/(2^14)   = 305.176 uV/bit
#define CONVERT_14BIT_TO_5V		0.000305176f
// 60V range & 14 bit ADC 	   - 60/(2^14)  = 3.6621 mV/bit
#define CONVERT_14BIT_TO_60V	0.0036621f


#define DATA_REFRESH_DELAY_MILLIS 100


/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */
// TODO add description
typedef enum
{
	SNA = 0,	// Value on startup
	GOOD,		// Data nominal
	BAD,		// Data was acquired but isn't trustworthy
	MIA			// Data wasn't aquired
} Bmb_Sensor_Status_E;

typedef enum
{
	MUX1 = 0,
	MUX2,
	MUX3,
	MUX4,
	MUX5,
	MUX6,
	MUX7,
	MUX8,
	NUM_MUXES
} Mux_State;


/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */
// TODO add description
typedef struct
{
	uint32_t numBricks;
	// TODO - set initial status value to SNA
	Bmb_Sensor_Status_E brickVStatus[NUM_BRICKS_PER_BMB];
	float brickV[NUM_BRICKS_PER_BMB];
	float stackV;

	Bmb_Sensor_Status_E blockVStatus;
	float blockV;

	Bmb_Sensor_Status_E tempStatus[NUM_BRICKS_PER_BMB+NUM_BOARD_TEMP_PER_BMB];
	float tempVoltage[NUM_BRICKS_PER_BMB+NUM_BOARD_TEMP_PER_BMB];
	float brickTemp[NUM_BRICKS_PER_BMB];
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


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */
/*!
  @brief   Initialize ASCI and BMB daisy chain. Enumerate BMBs
  @param   numBmbs - Updated with number of enumerated BMBs from HELLOALL command
  @return  True if successful initialization, false otherwise
*/
bool initASCI(uint32_t *numBmbs);

/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @return  True if successful initialization, false otherwise
*/
void initBmbs(uint32_t numBmbs);

// TODO update description
void cyclicUpdateBMBData(Bmb_S* bmb, uint32_t numBmbs);

// TODO update description
void updateBMBVoltageData(Bmb_S* bmb, uint32_t numBmbs);

// TODO update description
void updateBMBTempData(Bmb_S* bmb, uint32_t numBmbs);

void setMux(uint32_t numBmbs, uint8_t muxSetting);

// TODO update description
void setGpio(uint32_t numBmbs, bool gpio0, bool gpio1, bool gpio2, bool gpio3);

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBrickVoltages(Bmb_S* bmb,uint32_t numBmbs);


#endif /* INC_BMB_H_ */
