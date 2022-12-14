#ifndef INC_BMB_H_
#define INC_BMB_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define VERSION			0x00
#define ADDRESS			0x01
#define STATUS			0x02
#define FMEA1			0x03
#define ALRTCELL		0x04
#define ALRTOVCELL		0x05
#define ALRTUVCELL		0x07
#define ALRTBALSW		0x08
#define MINMAXCELL		0x0A
#define FMEA2			0x0B
#define AUTOBALSWDIS	0x0C
#define DEVCFG1			0x10
#define GPIO 			0x11
#define MEASUREEN 		0x12
#define SCANCTRL		0x13
#define WATCHDOG		0x18
#define ACQCFG			0x19
#define BALSWEN			0x1A
#define CELLn			0x20
#define VBLOCK			0x2C
#define AIN1			0x2D
#define AIN2			0x2E

#define NUM_BRICKS_PER_BMB		12
#define NUM_BOARD_TEMP_PER_BMB 	4

// 3.3V range & 12 bit reading - 3.3/(2^12) = 805.664 uV/bit
#define CONVERT_12BIT_TO_3V3	0.000805664f;
// 5V range & 14 bit reading   - 5/(2^14)   = 305.176 uV/bit
#define CONVERT_14BIT_TO_5V		0.000305176f
// 60V range & 14 bit ADC 	   - 60/(2^14)  = 3.6621 mV/bit
#define CONVERT_14BIT_TO_60V	0.0036621f


/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

// TODO add description
typedef enum
{
	SNA = 0,	// Value on startup
	GOOD,		// Data nominal
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
	NUM_MUX_CHANNELS
} Mux_State_E;


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

	// Balancing Configuration
	bool balSwRequested[NUM_BRICKS_PER_BMB];	// Set by BMS to determine which cells need to be balanced
	bool balSwEnabled[NUM_BRICKS_PER_BMB];		// Set by BMB based on ability to balance in hardware
} Bmb_S;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */



/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @return  True if successful initialization, false otherwise
*/
void initBmbs(uint32_t numBmbs);

/*!
  @brief   Update BMB voltages and temperature data. Once new data gathered start new
		   data acquisition scan
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void updateBmbData(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Only update voltage data on BMBs
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
// TODO - see whether or not this can be deleted
void updateBmbVoltageData(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Read all temperature channels on BMB
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
// TODO - see whether or not this can be deleted
void updateBmbTempData(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Set a given mux configuration on all BMBs
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   muxSetting - What mux setting should be used
*/
void setMux(uint32_t numBmbs, uint8_t muxSetting);

/*!
  @brief   Set the GPIO pins on the BMBs
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   gpio0 - True if GPIO should be high, false otherwise
  @param   gpio1 - True if GPIO should be high, false otherwise
  @param   gpio2 - True if GPIO should be high, false otherwise
  @param   gpio3 - True if GPIO should be high, false otherwise
*/
void setGpio(uint32_t numBmbs, bool gpio0, bool gpio1, bool gpio2, bool gpio3);

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBrickVoltages(Bmb_S* bmb,uint32_t numBmbs);

/*!
  @brief   Handles balancing the cells based on BMS control
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void balanceCells(Bmb_S* bmb, uint32_t numBmbs);


#endif /* INC_BMB_H_ */
