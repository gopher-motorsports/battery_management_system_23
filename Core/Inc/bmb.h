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
#define BALSHRTTHR		0x4B
#define BALLOWTHR 		0x4C
#define BALHIGHTHR 		0x4D
#define DIAG			0x50
#define DIAGCFG 		0x51

#define NUM_BRICKS_PER_BMB		12
#define NUM_BOARD_TEMP_PER_BMB 	4

#define NUM_BMB_FAULTS_PER_BMB	10			

// 3.3V range & 12 bit reading - 3.3/(2^12) = 805.664 uV/bit
#define CONVERT_12BIT_TO_3V3	0.000805664f;
// 5V range & 14 bit reading   - 5/(2^14)   = 305.176 uV/bit
#define CONVERT_14BIT_TO_5V		0.000305176f
// 60V range & 14 bit ADC 	   - 60/(2^14)  = 3.6621 mV/bit
#define CONVERT_14BIT_TO_60V	0.0036621f
// 2.307V range & 14 bit reading - 2.307/(2^14) = 140.808 uV/bit
#define CONVERT_14BIT_TO_VREF	0.000140808f;

// Die temp linear conversion gain = 3.07mv/°C
#define CONVERT_DIE_TEMP_GAIN	3.07f
// Die temp linear conversion offset = -273°C
#define CONVERT_DIE_TEMP_OFFSET	-273

// The minimum voltage that we can bleed to
#define MIN_BLEED_TARGET_VOLTAGE_V 	3.5f
// The maximum allowed board temp where bleeding is allowed
#define MAX_BOARD_TEMP_BALANCING_ALLOWED_C	75.0f
// The maximum cell temperature where bleeding is allowed
#define MAX_CELL_TEMP_BLEEDING_ALLOWED_C	55.0f

// Diagnostic bounds
#define ALTREF_MIN				0x3EF8
#define ALTREF_MAX				0x4034
#define VAA_MIN					3.27f
#define VAA_MAX					3.31f
#define LSAMP_MAX_OFFSET		0.2f
#define ZERO_SCALE_ADC_SUCCESS	0x0000
#define FULL_SCALE_ADC_SUCCESS	0xFFF0
#define DIE_TEMP_MAX			120


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
} Bmb_Mux_State_E;

typedef enum{
	BMB_REFERENCE_VOLTAGE_DIAGNOSTIC = 0,
	BMB_VAA_DIAGNOSTIC,
	BMB_LSAMP_OFFSET_DIAGNOSTIC,
	BMB_ADC_BIT_STUCK_HIGH_DIAGNOSTIC,
	BMB_ADC_BIT_STUCK_LOW_DIAGNOSTIC,
	BMB_DIE_TEMP_DIAGNOSTIC,
	NUM_BMB_DIAGNOSTIC_STATES
} Bmb_Diagnostic_State_E;

typedef enum
{
	BMB_REFERENCE_VOLTAGE_F = 0,
	BMB_VAA_F,
	BMB_LSAMP_OFFSET_F,
	BMB_ADC_BIT_STUCK_HIGH_F,
	BMB_ADC_BIT_STUCK_LOW_F,
	BMB_DIE_TEMP_F,
	BMB_BAL_SW_F,
	NUM_BMB_FAULTS
} Bmb_Fault_State_E;

typedef enum{
	SCAN_STACK = 0,
	VERIFY_BALSW_OPEN,
	VERIFY_BALSW_CLOSED,
	SCAN_ERROR
} Bmb_Scan_State_E;


/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

// TODO add description
typedef struct
{
	const uint32_t bmbIdx;
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

	// Balancing Configuration
	bool balSwRequested[NUM_BRICKS_PER_BMB];	// Set by BMS to determine which cells need to be balanced
	bool balSwEnabled[NUM_BRICKS_PER_BMB];		// Set by BMB based on ability to balance in hardware

	// Diagnostic information
	bool fault[NUM_BMB_FAULTS_PER_BMB];
	float dieTemp;
} Bmb_S;


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @return  True if successful initialization, false otherwise
*/
bool initBmbs(uint32_t numBmbs);

// /*!
//   @brief   Update BMB voltages and temperature data. Once new data gathered start new
// 		   data acquisition scan
//   @param   bmb - BMB array data
//   @param   numBmbs - The expected number of BMBs in the daisy chain
// */
// bool smartUpdateBmbData(Bmb_S* bmb, uint32_t numBmbs, bool requestDiagnostic);

void runBmbUpdate(Bmb_S* bmb, uint32_t numBmbs);

void aggregateBmbData(Bmb_S* bmb, uint32_t numBmbs);

/*!
  @brief   Handles balancing the cells based on BMS control
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void balanceCells(Bmb_S* bmb, uint32_t numBmbs);


#endif /* INC_BMB_H_ */
