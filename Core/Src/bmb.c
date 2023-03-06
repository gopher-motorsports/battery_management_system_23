/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "main.h"
#include "cmsis_os.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "bmbUtils.h"
#include "packData.h"
#include "debug.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define DATA_REFRESH_DELAY_MS 100
#define WATCHDOG_1S_STEP_SIZE 0x1000
#define WATCHDOG_TIMER_LOAD_5 0x0500
#define DEVCFG1_ENABLE_ALIVE_COUNTER	0x0040
#define DEVCFG1_DEFAULT_CONFIG			0x1002
#define MEASUREEN_ENABLE_BRICK_CHANNELS 0x0FFF
#define MEASUREEN_ENABLE_VBLOCK_CHANNEL 0xC000
#define MEASUREEN_ENABLE_AIN1_CHANNEL	0x1000
#define MEASUREEN_ENABLE_AIN2_CHANNEL	0x2000
#define ACQCFG_THRM_ON					0x0300
#define ACQCFG_MAX_SETTLING_TIME		0x003F
#define AUTOBALSWDIS_5MS_RECOVERY_TIME	0x0034
#define SCANCTRL_START_SCAN				0x0001
#define SCANCTRL_32_OVERSAMPLES			0x0040
#define SCANCTRL_ENABLE_AUTOBALSWDIS	0x0800


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

void updateBmbBalanceSwitches(Bmb_S* bmb);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/*!
  @brief   Enable the hardware bleed switches if balSwEnabled set in BMB struct
  @param   bmb - pointer to bmb that needs to be updated
*/
void updateBmbBalanceSwitches(Bmb_S* bmb)
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


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @return  True if successful initialization, false otherwise
*/
//TODO make this return bool
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
	if(!writeAll(SCANCTRL, (SCANCTRL_START_SCAN | SCANCTRL_32_OVERSAMPLES | SCANCTRL_ENABLE_AUTOBALSWDIS), numBmbs))
	{
		Debug("Failed to start initial scan!\n");
	}

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
	if((HAL_GetTick() - lastUpdate) >= DATA_REFRESH_DELAY_MS)
	{
		// Update lastUpdate
		lastUpdate = HAL_GetTick();

		// Verify that Scan completed successfully
		if (readAll(SCANCTRL, recvBuffer, numBmbs))
		{
			bool allBmbScanDone = true;
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				uint16_t scanCtrlData = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
				allBmbScanDone &= ((scanCtrlData & 0xA000) == 0xA000);	// Verify SCANDONE and DATARDY bits
			}
			if (!allBmbScanDone)
			{
				// Do we want to try to restart the scans here?
				Debug("All BMB Scans failed to complete in time\n");
				return;	
			}
		}
		else
		{
			// TODO - improve this. Handle failure correctly
			Debug("Failed to read SCANCTRL register\n");
			return;
		}

		// Update cell data
		for (uint8_t i = 0; i < 12; i++)
		{
			uint8_t cellReg = i + CELLn;
			if (readAll(cellReg, recvBuffer, numBmbs))
			{
				for (uint8_t j = 0; j < numBmbs; j++)
				{
					// Read brick voltage in [15:2]
					uint32_t brickVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
					float brickV = brickVRaw * CONVERT_14BIT_TO_5V;
					bmb[j].brickV[i] = brickV;
				}
			}
			else
			{
				Debug("Error during cellReg readAll!\n");

				// Failed to acquire data. Set status to MIA
				for (int32_t j = 0; j < numBmbs; j++)
				{
					bmb[j].brickVStatus[i] = MIA;
				}
			}
		}

		// Read VBLOCK register which is the sum of all brick voltages (stack voltage)
		if (readAll(VBLOCK, recvBuffer, numBmbs))
		{
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Read block voltage in [15:2]
				uint32_t stackVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float stackV = stackVRaw * CONVERT_14BIT_TO_60V;
				bmb[j].stackV = stackV;
			}
		}
		else
		{
			Debug("Error during VBLOCK readAll!\n");

			// Failed to acquire data. Set status to MIA
			for (int32_t j = 0; j < numBmbs; j++)
			{
				bmb[j].stackVStatus = MIA;
			}
		}

		// Read AUX/TEMP registers
		for (int32_t auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
		{
			if (readAll(auxChannel, recvBuffer, numBmbs))
			{
				// Parse received data
				for (uint8_t j = 0; j < numBmbs; j++)
				{
					// Read AUX voltage in [15:4]
					uint32_t auxRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 4;
					float auxV = auxRaw * CONVERT_12BIT_TO_3V3;
					bmb[j].tempVoltage[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = auxV;

					// Convert temp voltage registers to temperature readings
					if(muxState == MUX7 || muxState == MUX8) // NTC/ON-Board Temp Channel
					{
						bmb[j].boardTemp[muxState - MUX7 + ((auxChannel == AIN2) ? (NUM_BOARD_TEMP_PER_BMB/2) : 0)] = lookup(auxV, &ntcTable);
					}
					else // Zener/Brick Temp Channel
					{
						bmb[j].brickTemp[muxState + ((auxChannel == AIN2) ? (NUM_BRICKS_PER_BMB/2) : 0)] = lookup(auxV, &zenerTable);
					}
				}
			}
			else
			{
				Debug("Error during TEMP readAll!\n");
				// Failed to acquire data. Set status to MIA
				for (int32_t j = 0; j < numBmbs; j++)
				{
					bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = MIA;
				}
			}
		}

		// Cycle to next MUX configuration
		muxState = (muxState + 1) % NUM_MUX_CHANNELS;
		setMux(numBmbs, muxState);

		// Start acquisition for next function call with 32 oversamples and AUTOBALSWDIS
		if(!writeAll(SCANCTRL, 0x0841, numBmbs))
		{
			Debug("SHIT!\n");
		}
	}
}

/*!
  @brief   Only update voltage data on BMBs
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
// This should probably be rewritten
void updateBmbVoltageData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Start acquisition
	writeAll(SCANCTRL, 0x0001, numBmbs);

	// Update cell data
	for (uint8_t i = 0; i < 12; i++)
	{
		uint8_t cellReg = i + CELLn;
		if (readAll(cellReg, recvBuffer, numBmbs))
		{
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Read brick voltage in [15:2]
				uint32_t brickVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float brickV = brickVRaw * CONVERT_14BIT_TO_5V;
				bmb[j].brickV[i] = brickV;
			}
		}
		else
		{
			Debug("Error during cellReg readAll!\n");

			// Failed to acquire data. Set status to MIA
			for (int32_t j = 0; j < numBmbs; j++)
			{
				bmb[j].brickVStatus[i] = MIA;
			}
		}
	}

	// Read VBLOCK register which is the sum of all brick voltages (stack voltage)
		if (readAll(VBLOCK, recvBuffer, numBmbs))
		{
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Read block voltage in [15:2]
				uint32_t stackVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float stackV = stackVRaw * CONVERT_14BIT_TO_60V;
				bmb[j].stackV = stackV;
			}
		}
	else
	{
		Debug("Error during VBLOCK readAll!\n");

		// Failed to acquire data. Set status to MIA
		for (int32_t j = 0; j < numBmbs; j++)
		{
			bmb[j].stackVStatus = MIA;
		}
	}

	// TODO Add check - Compare VBLOCK with sum of brick voltages
}

/*!
  @brief   Read all temperature channels on BMB
  @param   bmb - BMB array data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void updateBmbTempData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Cycle through MUX channels
	for(Mux_State_E mux = MUX1; mux < NUM_MUX_CHANNELS; mux++)
	{
		// Set Mux configuration
		setMux(numBmbs, mux);
		// Read AUX/TEMP registers
		for (int32_t auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
		{
			if (readAll(auxChannel, recvBuffer, numBmbs))
			{
				// Parse received data
				for (uint8_t j = 0; j < numBmbs; j++)
				{
					// Read AUX voltage in [15:4]
					uint32_t auxRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 4;
					float auxV = auxRaw * CONVERT_12BIT_TO_3V3;
					bmb[j].tempVoltage[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = auxV;

					// Convert temp voltage registers to temperature readings
					if(muxState == MUX7 || muxState == MUX8) // NTC/ON-Board Temp Channel
					{
						bmb[j].boardTemp[muxState - MUX7 + ((auxChannel == AIN2) ? (NUM_BOARD_TEMP_PER_BMB/2) : 0)] = lookup(auxV, &ntcTable);
					}
					else // Zener/Brick Temp Channel
					{
						bmb[j].brickTemp[muxState + ((auxChannel == AIN2) ? (NUM_BRICKS_PER_BMB/2) : 0)] = lookup(auxV, &zenerTable);
					}
				}
			}
			else
			{
				// Failed to acquire data. Set status to MIA
				for (int32_t j = 0; j < numBmbs; j++)
				{
					bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = MIA;
				}
			}
		}
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
		float maxBrickTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBrickTemp = MAX_TEMP_SENSOR_VALUE_C;
		float brickTempSum = 0.0f;
		float maxBoardTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBoardTemp = MAX_TEMP_SENSOR_VALUE_C;
		float boardTempSum = 0.0f;
		// TODO If SNA do not count
		// Aggregate brick voltage and temperature data
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			float brickV = pBmb->brickV[j];
			float brickTemp = pBmb->brickTemp[j];

			if (brickV > maxBrickV)
			{
				maxBrickV = brickV;
			}
			if (brickV < minBrickV)
			{
				minBrickV = brickV;
			}

			if (brickTemp > maxBrickTemp)
			{
				maxBrickTemp = brickTemp;
			}
			if (brickTemp < minBrickTemp)
			{
				minBrickTemp = brickTemp;
			}

			sumV += brickV;
			brickTempSum += brickTemp;
		}

		// Aggregate board temp data
		for (int32_t j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
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

			boardTempSum += boardTemp;
		}

		// Update BMB statistics
		pBmb->maxBrickV = maxBrickV;
		pBmb->minBrickV = minBrickV;
		pBmb->sumBrickV	= sumV;
		pBmb->avgBrickV = sumV / NUM_BRICKS_PER_BMB;
		pBmb->maxBrickTemp = maxBrickTemp;
		pBmb->minBrickTemp = minBrickTemp;
		pBmb->avgBrickTemp = brickTempSum / NUM_BRICKS_PER_BMB;
		pBmb->maxBoardTemp = maxBoardTemp;
		pBmb->minBoardTemp = minBoardTemp;
		pBmb->avgBoardTemp = boardTempSum / NUM_BOARD_TEMP_PER_BMB;
	}
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
		Debug("Error during STATUS readAll!\n");
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
