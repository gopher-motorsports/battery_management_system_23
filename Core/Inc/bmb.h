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

#define BMB_DATA_REFRESH_DELAY_MS 50

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
#define DEVCFG2			0x1B
#define CELLn			0x20
#define VBLOCK			0x2C
#define AIN1			0x2D
#define AIN2			0x2E
#define ALRTRST			0x8000

#define NUM_BRICKS_PER_BMB		12
#define NUM_BOARD_TEMP_PER_BMB 	4

// 3.3V range & 12 bit reading - 3.3/(2^12) = 805.664 uV/bit
#define CONVERT_12BIT_TO_3V3				0.000805664f;
// 5V range & 14 bit reading   - 5/(2^14)   = 305.176 uV/bit
#define CONVERT_14BIT_TO_5V					0.000305176f
// 60V range & 14 bit reading  - 60/(2^14)  = 3.6621 mV/bit
#define CONVERT_14BIT_TO_60V				0.0036621f

// The minimum voltage that we can bleed to
#define MIN_BLEED_TARGET_VOLTAGE_V 			3.5f
// The maximum allowed board temp where bleeding is allowed
#define MAX_BOARD_TEMP_BALANCING_ALLOWED_C	90.0f
// The maximum cell temperature where bleeding is allowed
#define MAX_CELL_TEMP_BLEEDING_ALLOWED_C	55.0f


/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

// TODO add description
typedef enum
{
	UNINITIALIZED = 0,	// Value on startup
	GOOD,				// Data nominal
	BAD					// Unavailable data or hardware issue
} Sensor_Status_E;

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
	const uint32_t bmbIdx;
	uint32_t numBricks;
	// TODO - set initial status value to UNINITIALIZED
	// The status of all the brick sensors
	Sensor_Status_E brickVStatus[NUM_BRICKS_PER_BMB];
	// The brick voltages for the bmb
	float brickV[NUM_BRICKS_PER_BMB];
	
	// The resistance of the brick
	float brickResistance[NUM_BRICKS_PER_BMB];

	// The segment voltage measurement status 
	Sensor_Status_E segmentVStatus;
	// The segment voltage measurement is the total BMB voltage
	float segmentV;

	// The status of the brick temp sensors
	Sensor_Status_E brickTempStatus[NUM_BRICKS_PER_BMB];
	// The converted temperatures for all the brick temp sensors
	float brickTemp[NUM_BRICKS_PER_BMB];
	
	// The status of the board temp sensors
	Sensor_Status_E boardTempStatus[NUM_BOARD_TEMP_PER_BMB];
	// The converted temperature for all board temp sensors
	float boardTemp[NUM_BOARD_TEMP_PER_BMB];

	float sumBrickV;

	float maxBrickV;
	float minBrickV;
	float avgBrickV;
	uint32_t numBadBrickV;

	float maxBrickTemp;
	float minBrickTemp;
	float avgBrickTemp;
	uint32_t numBadBrickTemp;

	float maxBoardTemp;
	float minBoardTemp;
	float avgBoardTemp;
	uint32_t numBadBoardTemp;

	// Indicates that a BMB reinitialization is required
	bool reinitRequired;

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
  @brief   Set a given mux configuration on all BMBs
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   muxSetting - What mux setting should be used
*/
void setMux(uint32_t numBmbs, uint8_t muxSetting);

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBmbData(Bmb_S* bmb,uint32_t numBmbs);


/*!
  @brief   Set or clear the internal loopback mode for a specific BMB.
  @param   bmbIdx - The index of the BMB to configure.
  @param   enabled - True to enable internal loopback mode, false to disable it.
  @return  True if the internal loopback mode was set successfully, false otherwise.
*/
bool setBmbInternalLoopback(uint32_t bmbIdx, bool enabled);

/*!
  @brief   Determine where a BMB daisy chain break has occured
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @returns bmb index of BMB where communication failed (1 indexed)
		   if no break detected returns 0
*/
int32_t detectBmbDaisyChainBreak(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Determine if a power-on reset (POR) occurred and if so properly reset the device
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void detectPowerOnReset(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Handles balancing the cells based on BMS control
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void balanceCells(Bmb_S* bmb, uint32_t numBmbs);


#endif /* INC_BMB_H_ */
