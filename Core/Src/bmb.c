/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "main.h"
#include "cmsis_os.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "bmbUtils.h"
#include "packData.h"
#include "lookupTable.h"
#include "debug.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define BITS_IN_BYTE					8
#define RAILED_MARGIN_COUNT	  			200
#define MAX_12_BIT						0x0FFF
#define MAX_14_BIT						0x3FFF
#define WATCHDOG_1S_STEP_SIZE 			0x1000
#define WATCHDOG_TIMER_LOAD_5 			0x0500
#define DEVCFG1_ENABLE_ALIVE_COUNTER	0x0040
#define DEVCFG1_DEFAULT_CONFIG			0x1002
#define MEASUREEN_ENABLE_BRICK_CHANNELS 0x0FFF
#define MEASUREEN_ENABLE_VBLOCK_CHANNEL 0xC000
#define MEASUREEN_ENABLE_AIN1_CHANNEL	0x1000
#define MEASUREEN_ENABLE_AIN2_CHANNEL	0x2000
#define ACQCFG_THRM_ON					0x0300
#define ACQCFG_MAX_SETTLING_TIME		0x003F
#define AUTOBALSWDIS_5MS_RECOVERY_TIME	0x0034
#define DEVCFG2_LASTLOOP				0x8000
#define SCANCTRL_START_SCAN				0x0001
#define SCANCTRL_32_OVERSAMPLES			0x0040
#define SCANCTRL_ENABLE_AUTOBALSWDIS	0x0800
#define VERSION_DEFAULT_CONTENT			0x843


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */
static Mux_State_E muxState = MUX1;
static uint32_t lastUpdate = 0;
static uint8_t recvBuffer[SPI_BUFF_SIZE];


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern LookupTable_S ntcTable;
extern LookupTable_S zenerTable;

/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

static uint16_t getValueFromBuffer(uint8_t* buffer, uint32_t index);

static void updateBmbBalanceSwitches(Bmb_S* bmb);

static bool startScan(uint32_t numBmbs);

static bool is12BitSensorRailed(uint32_t rawAdcVal);

static bool is14BitSensorRailed(uint32_t rawAdcVal);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/*!
  @brief   Retrieve a 16-bit value from a buffer at the specified index.
  @param   buffer - Pointer to the buffer containing the data.
  @param   index - The index of the 16-bit value to retrieve.
  @return  The 16-bit value located at the specified index in the buffer.
*/
static uint16_t getValueFromBuffer(uint8_t* buffer, uint32_t index)
{
	return buffer[4 + 2*index] << BITS_IN_BYTE | buffer[3 + 2*index];
}

/*!
  @brief   Enable the hardware bleed switches if balSwEnabled set in BMB struct
  @param   bmb - pointer to bmb that needs to be updated
*/
static void updateBmbBalanceSwitches(Bmb_S* bmb)
{
	// TODO - should the watchdog be set in this function? For example if we want to ensure that all
	// bleeding is ended we would call this update function but there would be no need to update the watchdog
	// Set cell balancing watchdog timeout to 5s
	writeDevice(WATCHDOG, (WATCHDOG_1S_STEP_SIZE | WATCHDOG_TIMER_LOAD_5), bmb->bmbIdx);
	uint16_t balanceSwEnabled = 0x0000;
	uint16_t mask = 0x0001;
	for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
	{
		if (bmb->balSwEnabled[i])
		{
			balanceSwEnabled |= mask;
		}
		mask = mask << 1;
	}
	// Update the balance switches on the relevant BMB
	writeDevice(BALSWEN, balanceSwEnabled, bmb->bmbIdx);
}

/*!
  @brief   Set or clear the internal loopback mode for a specific BMB.
  @param   bmbIdx - The index of the BMB to configure.
  @param   enabled - True to enable internal loopback mode, false to disable it.
  @return  True if the internal loopback mode was set successfully, false otherwise.
*/
bool setBmbInternalLoopback(uint32_t bmbIdx, bool enabled)
{
	for (int i = 0; i < NUM_DATA_CHECKS; i++)
	{
		clearRxBuffer();
		// Set internal loopback mode for a given BMB
		writeDevice(DEVCFG2, enabled ? DEVCFG2_LASTLOOP : 0, bmbIdx);
		clearRxBuffer();
		// Verify that the internal loopback mode was enabled successfully
		readDevice(DEVCFG2, recvBuffer, bmbIdx);
		uint16_t registerValue = getValueFromBuffer(recvBuffer, 0);
		if (registerValue & DEVCFG2_LASTLOOP)
		{
			// Successfully wrote to LASTLOOP bit
			return true;
		}
	}
	return false;
}

/*!
  @brief   Start a scan for all BMBs in the daisy chain
  @param   numBmbs - The number of BMBs in the daisy chain.
  @return  True if the scan started successfully, false otherwise.
*/
static bool startScan(uint32_t numBmbs)
{
	const uint16_t scanCtrlData = SCANCTRL_ENABLE_AUTOBALSWDIS | SCANCTRL_32_OVERSAMPLES | SCANCTRL_START_SCAN;
	if(!writeAll(SCANCTRL, scanCtrlData, numBmbs))
	{
		DebugComm("Failed to start scan!\n");
		return false;
	}
	return true;
}

/*!
  @brief   Check if a 12-bit ADC sensor value is railed (near minimum or maximum).
  @param   rawAdcVal - The raw 12-bit ADC sensor value to check.
  @return  True if the sensor value is within the margin of railed values, false otherwise.
*/
static bool is12BitSensorRailed(uint32_t rawAdcVal)
{
	// Ensure only 12 bit value
	rawAdcVal &= MAX_12_BIT;

	// Determine if the adc reading is within a margin of the railed values
	if ((rawAdcVal < RAILED_MARGIN_COUNT) || (rawAdcVal > (MAX_12_BIT - RAILED_MARGIN_COUNT)))
	{
		return true;
	}

	return false;
}

/*!
  @brief   Check if a 14-bit ADC sensor value is railed (near minimum or maximum).
  @param   rawAdcVal - The raw 14-bit ADC sensor value to check.
  @return  True if the sensor value is within the margin of railed values, false otherwise.
*/
static bool is14BitSensorRailed(uint32_t rawAdcVal)
{
	// Ensure only 14 bit value
	rawAdcVal &= MAX_14_BIT;

	// Determine if the adc reading is within a margin of the railed values
	if ((rawAdcVal < RAILED_MARGIN_COUNT) || (rawAdcVal > (MAX_14_BIT - RAILED_MARGIN_COUNT)))
	{
		return true;
	}

	return false;
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void initBmbs(uint32_t numBmbs)
{
	// TODO - do we want to read the register contents back and verify values?
	// Enable alive counter byte
	// numBmbs set to 0 since alive counter not yet enabled
	writeAll(DEVCFG1, (DEVCFG1_DEFAULT_CONFIG | DEVCFG1_ENABLE_ALIVE_COUNTER), 0);

	// Enable measurement channels
	writeAll(MEASUREEN, (MEASUREEN_ENABLE_BRICK_CHANNELS | MEASUREEN_ENABLE_VBLOCK_CHANNEL | MEASUREEN_ENABLE_AIN1_CHANNEL | MEASUREEN_ENABLE_AIN2_CHANNEL), numBmbs);

	// Manual set THRM HIGH and config settling time
	writeAll(ACQCFG, (ACQCFG_THRM_ON | ACQCFG_MAX_SETTLING_TIME), numBmbs);

	// Enable 5ms delay between balancing and aquisition
	writeAll(AUTOBALSWDIS, AUTOBALSWDIS_5MS_RECOVERY_TIME, numBmbs);

	// Reset MUX configuration to Channel 1 - 000
	setMux(numBmbs, MUX1);

	// Start initial acquisition with 32 oversamples
	startScan(numBmbs);

	// Set brickOV voltage alert threshold
	// Set brickUV voltage alert threshold
}

/*!
  @brief   Update BMB voltages and temperature data. Once new data gathered start new
		   data acquisition scan
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void updateBmbData(Bmb_S* bmb, uint32_t numBmbs)
{
	if((HAL_GetTick() - lastUpdate) >= BMB_DATA_REFRESH_DELAY_MS)
	{
		// Update lastUpdate
		lastUpdate = HAL_GetTick();

		// Verify that Scan completed successfully
		if (readAll(SCANCTRL, recvBuffer, numBmbs))
		{
			bool allBmbScanDone = true;
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Extract register contents from receive buffer
				uint16_t scanCtrlData = getValueFromBuffer(recvBuffer, j);
				allBmbScanDone &= ((scanCtrlData & 0xA000) == 0xA000);	// Verify SCANDONE and DATARDY bits
			}
			if (!allBmbScanDone)
			{
				// Restart scans since scan may have not started properly
				startScan(numBmbs);
				// TODO: Set sensor status to bad
				DebugComm("All BMB Scans failed to complete in time\n");
				return;	
			}
		}
		else
		{
			// TODO - improve this. Handle failure correctly
			DebugComm("Failed to read SCANCTRL register\n");
			return;
		}

		// Update brick voltage data
		for (uint8_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
		{
			uint8_t cellReg = i + CELLn;
			if (readAll(cellReg, recvBuffer, numBmbs))
			{
				for (uint8_t j = 0; j < numBmbs; j++)
				{
					// Read brick voltage in [15:2]
					uint32_t brickVRaw = getValueFromBuffer(recvBuffer, j) >> 2;
					float brickV = brickVRaw * CONVERT_14BIT_TO_5V;
					// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
					const uint32_t bmbIdx = numBmbs - j - 1;
					bmb[bmbIdx].brickV[i] = brickV;
					bmb[bmbIdx].brickVStatus[i] = is14BitSensorRailed(brickVRaw) ? BAD : GOOD;
				}
			}
			else
			{
				DebugComm("Error during cellReg readAll!\n");

				// Failed to acquire data. Set status to BAD
				for (int32_t j = 0; j < numBmbs; j++)
				{
					// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
					const uint32_t bmbIdx = numBmbs - j - 1;
					bmb[bmbIdx].brickVStatus[i] = BAD;
				}
			}
		}

		// Read VBLOCK register which is the total voltage of the segment
		if (readAll(VBLOCK, recvBuffer, numBmbs))
		{
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Read block voltage in [15:2]
				uint32_t segmentVRaw = getValueFromBuffer(recvBuffer, j) >> 2;
				float segmentV = segmentVRaw * CONVERT_14BIT_TO_60V;
				// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
				const uint32_t bmbIdx = numBmbs - j - 1;
				bmb[bmbIdx].segmentV = segmentV;
				bmb[bmbIdx].segmentVStatus = is14BitSensorRailed(segmentVRaw) ? BAD : GOOD;
			}
		}
		else
		{
			DebugComm("Error during VBLOCK readAll!\n");

			// Failed to acquire data. Set status to BAD
			for (int32_t j = 0; j < numBmbs; j++)
			{
				// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
				const uint32_t bmbIdx = numBmbs - j - 1;
				bmb[bmbIdx].segmentVStatus = BAD;
			}
		}

		// Read AUX/TEMP registers
		for (int32_t auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
		{
			if (readAll(auxChannel, recvBuffer, numBmbs))
			{
				// Parse received data
				for (uint32_t j = 0; j < numBmbs; j++)
				{
					// Read AUX voltage in [15:4]
					uint32_t auxRaw = getValueFromBuffer(recvBuffer, j) >> 4;
					float auxV = auxRaw * CONVERT_12BIT_TO_3V3;

					// Convert temp voltage registers to temperature readings
					if(muxState == MUX7 || muxState == MUX8) // NTC/ON-Board Temp Channel
					{
						// Ternary statements used to resolve index of NTC channel from mux position and ain port
						// NTC1: MUX7 (1) + AIN2 (-1)	= Index 0
						// NTC2: MUX7 (1) + AIN1 (0)	= Index 1
						// NTC3: MUX8 (3) + AIN2 (-1)	= Index 2
						// NTC4: MUX8 (3) + AIN1 (0)	= Index 3
						const uint32_t ntcIdx = ((muxState == MUX7) ? 1 : 3) + ((auxChannel == AIN1) ? 0 : -1);
						// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
						const uint32_t bmbIdx = numBmbs - j - 1;
						bmb[bmbIdx].boardTemp[ntcIdx] = lookup(auxV, &ntcTable);
						bmb[bmbIdx].boardTempStatus[ntcIdx] = is12BitSensorRailed(auxRaw) ? BAD : GOOD;
						// TODO Add board temp status
					}
					else // Zener/Brick Temp Channel
					{
						const uint32_t brickIdx = muxState + ((auxChannel == AIN2) ? (NUM_BRICKS_PER_BMB/2) : 0);
						// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
						const uint32_t bmbIdx = numBmbs - j - 1;
						bmb[bmbIdx].brickTemp[brickIdx] = lookup(auxV, &zenerTable);
						bmb[bmbIdx].brickTempStatus[brickIdx] = is12BitSensorRailed(auxRaw) ? BAD : GOOD;
					}
				}
			}
			else
			{
				DebugComm("Error during TEMP readAll!\n");
				// Failed to acquire data. Set status to BAD
				for (int32_t j = 0; j < numBmbs; j++)
				{
					if(muxState == MUX7 || muxState == MUX8) // NTC/ON-Board Temp Channel
					{
						// Ternary statements used to resolve index of NTC channel from mux position and ain port
						// NTC1: MUX7 (1) + AIN2 (-1)	= Index 0
						// NTC2: MUX7 (1) + AIN1 (0)	= Index 1
						// NTC3: MUX8 (3) + AIN2 (-1)	= Index 2
						// NTC4: MUX8 (3) + AIN1 (0)	= Index 3
						const uint32_t ntcIdx = ((muxState == MUX7) ? 1 : 3) + ((auxChannel == AIN1) ? 0 : -1);
						// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
						const uint32_t bmbIdx = numBmbs - j - 1;
						bmb[bmbIdx].boardTempStatus[ntcIdx] = BAD;
					}
					else
					{
						const uint32_t brickIdx = muxState + ((auxChannel == AIN2) ? (NUM_BRICKS_PER_BMB/2) : 0);
						// Convert from frame index (starts with last BMB) to bmb index (starts with first BMB) 
						const uint32_t bmbIdx = numBmbs - j - 1;
						bmb[bmbIdx].brickTempStatus[brickIdx] = BAD;
					}
				}
			}
		}

		// Cycle to next MUX configuration
		muxState = (muxState + 1) % NUM_MUX_CHANNELS;
		setMux(numBmbs, muxState);

		// Start acquisition for next function call with 32 oversamples and AUTOBALSWDIS
		startScan(numBmbs);
	}
}

/*!
  @brief   Set a given mux configuration on all BMBs
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   muxSetting - What mux setting should be used
*/
void setMux(uint32_t numBmbs, uint8_t muxSetting)
{
	// Last 3 bits set GPIO logic state for channels 2, 1, 0 respectively
	uint16_t gpioData = 0xF000 | (muxSetting & 0x07);
	writeAll(GPIO, gpioData, numBmbs);
}

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBmbData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Iterate through brick voltages
	for (int32_t i = 0; i < numBmbs; i++)
	{
		Bmb_S* pBmb = &bmb[i];
		float maxBrickV = MIN_VOLTAGE_SENSOR_VALUE_V;
		float minBrickV = MAX_VOLTAGE_SENSOR_VALUE_V;
		float sumV	= 0.0f;
		uint32_t numGoodBrickV = 0;

		float maxBrickTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBrickTemp = MAX_TEMP_SENSOR_VALUE_C;
		float brickTempSum = 0.0f;
		uint32_t numGoodBrickTemp = 0;

		float maxBoardTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBoardTemp = MAX_TEMP_SENSOR_VALUE_C;
		float boardTempSum = 0.0f;
		uint32_t numGoodBoardTemp = 0;

		// Aggregate brick voltage and temperature data
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			// Only update stats if sense status is good
			if (pBmb->brickVStatus[j] == GOOD)
			{
				float brickV = pBmb->brickV[j];

				if (brickV > maxBrickV)
				{
					maxBrickV = brickV;
				}
				if (brickV < minBrickV)
				{
					minBrickV = brickV;
				}
				numGoodBrickV++;
				sumV += brickV;
			}

			// Only update stats if sense status is good
			if (pBmb->brickTempStatus[j] == GOOD)
			{
				float brickTemp = pBmb->brickTemp[j];

				if (brickTemp > maxBrickTemp)
				{
					maxBrickTemp = brickTemp;
				}
				if (brickTemp < minBrickTemp)
				{
					minBrickTemp = brickTemp;
				}
				numGoodBrickTemp++;
				brickTempSum += brickTemp;
			}
		}

		// Aggregate board temp data
		for (int32_t j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
		{
			if (pBmb->boardTempStatus[j] == GOOD)
			{
				float boardTemp = pBmb->boardTemp[j];

				if (boardTemp > maxBoardTemp)
				{
					maxBoardTemp = boardTemp;
				}
				if (boardTemp < minBoardTemp)
				{
					minBoardTemp = boardTemp;
				}
				numGoodBoardTemp++;
				boardTempSum += boardTemp;
			}
		}

		// Update BMB statistics
		pBmb->maxBrickV = maxBrickV;
		pBmb->minBrickV = minBrickV;
		pBmb->sumBrickV	= sumV;
		pBmb->avgBrickV = (numGoodBrickV == 0) ? pBmb->avgBrickV : sumV / numGoodBrickV;
		pBmb->numBadBrickV = NUM_BRICKS_PER_BMB - numGoodBrickV;

		pBmb->maxBrickTemp = maxBrickTemp;
		pBmb->minBrickTemp = minBrickTemp;
		pBmb->avgBrickTemp = (numGoodBrickTemp == 0) ? pBmb->avgBrickTemp :  brickTempSum / numGoodBrickTemp;
		pBmb->numBadBrickTemp = NUM_BRICKS_PER_BMB - numGoodBrickTemp;

		pBmb->maxBoardTemp = maxBoardTemp;
		pBmb->minBoardTemp = minBoardTemp;
		pBmb->avgBoardTemp = (numGoodBoardTemp == 0) ? pBmb->avgBoardTemp : boardTempSum / numGoodBoardTemp;
		pBmb->numBadBoardTemp = NUM_BOARD_TEMP_PER_BMB - numGoodBoardTemp;
	}
}


/*!
  @brief   Determine where a BMB daisy chain break has occured
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @returns bmb index of BMB where communication failed (1 indexed)
		   if no break detected returns 0
*/
int32_t detectBmbDaisyChainBreak(Bmb_S* bmb, uint32_t numBmbs)
{
	// Iterate through BMB indexes and determine if a daisy chain break exists
	for (uint32_t bmbIdx = 0; bmbIdx < numBmbs; bmbIdx++)
	{
		if (!setBmbInternalLoopback(bmbIdx, true)) { return bmbIdx + 1; }

		// Determine if loopback communication is restored
		memset(recvBuffer, 0, sizeof(recvBuffer));
		readAll(VERSION, recvBuffer, bmbIdx + 1);

		for (uint32_t i = 0; i < bmbIdx + 1; i++)
		{
			// Read model number in [15:4] to verify succesful communication
			uint32_t versionRegister = getValueFromBuffer(recvBuffer, i) >> 4;
			if (versionRegister != VERSION_DEFAULT_CONTENT)
			{
				// Broken link detected. Convert bmbIdx from 0-indexed to 1-indexed value 
				return bmbIdx + 1;
			}
		}

		// No issue detected yet. Disable internal loopback and continue looking
		if (!setBmbInternalLoopback(bmbIdx, false)) { return bmbIdx + 1; }
	}
	// No errors detected - enable internal loopback on final BMB
	if (!setBmbInternalLoopback(numBmbs-1, true)) { return -1; }
	return 0;
}


/*!
  @brief   Determine if a power-on reset (POR) occurred and if so properly reset the device
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void detectPowerOnReset(Bmb_S* bmb, uint32_t numBmbs)
{
	if (readAll(STATUS, recvBuffer, numBmbs))
	{
		for (uint8_t j = 0; j < numBmbs; j++)
		{
			// Read ALRTRST in STATUS [15]
			const bool por = (recvBuffer[3 + j] & ALRTRST) != 0;
			if (por)
			{
				bmb[j].reinitRequired = true;
			}
		}
	}
	else
	{
		DebugComm("Error during STATUS readAll!\n");
	}
}

/*!
  @brief   Determine which bricks need to be balanced
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void balanceCells(Bmb_S* bmb, uint32_t numBmbs)
{
	// To determine cell balancing priority add all cells that need to be balanced to an array
	// Sort this array by cell voltage. We always want to balance the highest cell voltage. 
	// Iterate through the array starting with the highest voltage and enable balancing switch
	// if neighboring cells aren't being balanced. This is due to the circuit not allowing 
	// neighboring cells to be balanced. 
	for (int32_t bmbIdx = 0; bmbIdx < numBmbs; bmbIdx++)
	{
		uint32_t numBricksNeedBalancing = 0;
		Brick_S bricksToBalance[NUM_BRICKS_PER_BMB];
		// Add all bricks that need balancing to array if board temp allows for it
		if (bmb[bmbIdx].maxBoardTemp < MAX_BOARD_TEMP_BALANCING_ALLOWED_C)
		{
			for (int32_t brickIdx = 0; brickIdx < NUM_BRICKS_PER_BMB; brickIdx++)
			{
				// Add brick to list of bricks that need balancing if balancing requested, brick
				// isn't too hot, and the brick voltage is above the bleed threshold
				if (bmb[bmbIdx].balSwRequested[brickIdx] &&
					bmb[bmbIdx].brickTemp[brickIdx] < MAX_CELL_TEMP_BLEEDING_ALLOWED_C &&
					bmb[bmbIdx].brickV[brickIdx] > MIN_BLEED_TARGET_VOLTAGE_V)
				{
					// Brick needs to be balanced, add to array
					bricksToBalance[numBricksNeedBalancing++] = (Brick_S) { .brickIdx = brickIdx, .brickV = bmb[bmbIdx].brickV[brickIdx] };
				}
			}
		}
		// Sort array of bricks that need balancing by their voltage
		insertionSort(bricksToBalance, numBricksNeedBalancing);
		// Clear all balance switches
		memset(bmb[bmbIdx].balSwEnabled, 0, NUM_BRICKS_PER_BMB * sizeof(bool));

		for (int32_t i = numBricksNeedBalancing - 1; i >= 0; i--)
		{
			// For each brick that needs balancing ensure that the neighboring bricks aren't being bled
			Brick_S brick = bricksToBalance[i];
			int leftIdx = brick.brickIdx - 1;
			int rightIdx = brick.brickIdx + 1;
			bool leftNotBalancing = false;
			bool rightNotBalancing = false;
			if (leftIdx < 0)
			{
				leftNotBalancing = true;
			}
			else if (!bmb[bmbIdx].balSwEnabled[leftIdx])
			{
				leftNotBalancing = true;
			}

			if (rightIdx >= NUM_BRICKS_PER_BMB)
			{
				rightNotBalancing = true;
			}
			else if (!bmb[bmbIdx].balSwEnabled[rightIdx])
			{
				rightNotBalancing = true;
			}

			if (leftNotBalancing && rightNotBalancing)
			{
				bmb[bmbIdx].balSwEnabled[brick.brickIdx] = true;
			}
		}
		// Update the BMB balance switches in hardware
		updateBmbBalanceSwitches(&bmb[bmbIdx]);
	}
}
