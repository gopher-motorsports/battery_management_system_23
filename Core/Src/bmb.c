/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "main.h"
#include "cmsis_os.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "bmbUtils.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define DATA_REFRESH_DELAY_MS 100


/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */
static Mux_State_E muxState = 0x00;
static bool gpio3State = 0;
static uint32_t lastUpdate = 0;
uint8_t recvBuffer[SPI_BUFF_SIZE];
uint8_t sendBuffer[SPI_BUFF_SIZE];


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */
extern osSemaphoreId binSemHandle;


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
	writeAll(DEVCFG1, 0x1042, 0);

	// Enable measurement channels
	writeAll(MEASUREEN, 0xFFFF, numBmbs);

	// Manual set THRM HIGH and config settling time
	writeAll(ACQCFG, 0xFFFF, numBmbs);

	// Enable 5ms delay between balancing and aquisition
	writeAll(AUTOBALSWDIS, 0x0033, numBmbs);

	// Reset GPIO to 0 state
	setGpio(numBmbs, 0, 0, 0, 0);


	// Start initial acquisition with 32 oversamples
	if(!writeAll(SCANCTRL, 0x0841, numBmbs))
	{
		printf("SHIT!\n");
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
				// Read brick voltage in [15:2]
				uint16_t scanCtrlData = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
				allBmbScanDone &= ((scanCtrlData & 0xA000) == 0xA000);	// Verify SCANDONE and DATARDY bits
			}
			if (!allBmbScanDone)
			{
				printf("All BMB Scans failed to complete in time\n");
				return;
			}
		}
		else
		{
			// TODO - improve this. Handle failure correctly
			printf("Failed to read SCANCTRL register\n");
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
				printf("Error during cellReg readAll!\n");

				// Failed to acquire data. Set status to MIA
				for (int j = 0; j < numBmbs; j++)
				{
					bmb[j].brickVStatus[i] = MIA;
				}
			}
		}

		// Read VBLOCK register
		if (readAll(VBLOCK, recvBuffer, numBmbs))
		{
			for (uint8_t j = 0; j < numBmbs; j++)
			{
				// Read block voltage in [15:2]
				uint32_t blockVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float blockV = blockVRaw * CONVERT_14BIT_TO_60V;
				bmb[j].blockV = blockV;
			}
		}
		else
		{
			printf("Error during VBLOCK readAll!\n");

			// Failed to acquire data. Set status to MIA
			for (int j = 0; j < numBmbs; j++)
			{
				bmb[j].blockVStatus = MIA;
			}
		}

		// Read AUX/TEMP registers
		for (int auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
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
				printf("Error during TEMP readAll!\n");
				// Failed to acquire data. Set status to MIA
				for (int j = 0; j < numBmbs; j++)
				{
					bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = MIA;
				}
			}
		}

		// Cycle to next MUX configuration
		setMux(numBmbs, (muxState + 1) % NUM_MUX_CHANNELS);

		// Start acquisition for next function call with 32 oversamples and AUTOBALSWDIS
		if(!writeAll(SCANCTRL, 0x0841, numBmbs))
		{
			printf("SHIT!\n");
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
			printf("Error during cellReg readAll!\n");

			// Failed to acquire data. Set status to MIA
			for (int j = 0; j < numBmbs; j++)
			{
				bmb[j].brickVStatus[i] = MIA;
			}
		}
	}

	// Read VBLOCK register
	if (readAll(VBLOCK, recvBuffer, numBmbs))
	{
		for (uint8_t j = 0; j < numBmbs; j++)
		{
			// Read block voltage in [15:2]
			uint32_t blockVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
			float blockV = blockVRaw * CONVERT_14BIT_TO_60V;
			bmb[j].blockV = blockV;
		}
	}
	else
	{
		printf("Error during VBLOCK readAll!\n");

		// Failed to acquire data. Set status to MIA
		for (int j = 0; j < numBmbs; j++)
		{
			bmb[j].blockVStatus = MIA;
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
		for (int auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
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
				for (int j = 0; j < numBmbs; j++)
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
	bool gpio[3];
	for(int i = 0; i < 3; i++)
	{
		gpio[i] = (muxSetting >> i) & 1;
	}
	setGpio(numBmbs, gpio[0], gpio[1], gpio[2], gpio3State); // Currently sets GPIO 4 to 0 when updating MUX
}

/*!
  @brief   Set the GPIO pins on the BMBs
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   gpio0 - True if GPIO should be high, false otherwise
  @param   gpio1 - True if GPIO should be high, false otherwise
  @param   gpio2 - True if GPIO should be high, false otherwise
  @param   gpio3 - True if GPIO should be high, false otherwise
*/
void setGpio(uint32_t numBmbs, bool gpio0, bool gpio1, bool gpio2, bool gpio3)
{
	// First 4 bits set GPIO to output mode
	// Last 4 bits set GPIO logic state for channels 3, 2, 1, 0 respectively
	uint16_t data = 0xF000 | (gpio3 << 3) | (gpio2 << 2) | (gpio1 << 1) | (gpio0);
	writeAll(GPIO, data, numBmbs);

	// Update Mux state depending on only GPIO settings 0, 1, 2
	muxState = data & 0x0007;

	// Update gpio4 state
	gpio3State = gpio3;
}

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBrickVoltages(Bmb_S* bmb, uint32_t numBmbs)
{
	for (int i = 0; i < numBmbs; i++)
	{
		Bmb_S* pBmb = &bmb[i];
		float maxBrickV = 0.0f;
		float minBrickV = 5.0f;
		float stackV	= 0.0f;
		// TODO If SNA do not count
		for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
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

			stackV += brickV;
		}
		pBmb->maxBrickV = maxBrickV;
		pBmb->minBrickV = minBrickV;
		pBmb->stackV	= stackV;
		pBmb->avgBrickV = stackV / NUM_BRICKS_PER_BMB;
	}
}

/*!
  @brief   Handles balancing the cells based on BMS control
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
	for (int bmbIdx = 0; bmbIdx < numBmbs; bmbIdx++)
	{
		uint32_t numBricksNeedBalancing = 0;
		Brick_S bricksToBalance[NUM_BRICKS_PER_BMB];
		// Add all bricks that need balancing to array
		for (int brickIdx = 0; brickIdx < NUM_BRICKS_PER_BMB; brickIdx++)
		{
			if (bmb[bmbIdx].balSwRequested[brickIdx])
			{
				// Brick needs to be balanced, add to array
				bricksToBalance[numBricksNeedBalancing++] = (Brick_S) { .brickIdx = brickIdx, .brickV = bmb[bmbIdx].brickV[brickIdx] };
			}
		}
		// Sort array of bricks that need balancing by their voltage
		insertionSort(bricksToBalance, numBricksNeedBalancing);
		// Clear all balance switches
		memset(bmb[bmbIdx].balSwEnabled, 0, NUM_BRICKS_PER_BMB * sizeof(bool));

		for (int i = numBricksNeedBalancing - 1; i >= 0; i--)
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
		// Set cell balancing watchdog timeout to 5s
		writeDevice(WATCHDOG, 0x1500, bmbIdx);
		uint16_t balanceSwEnabled = 0x0000;
		uint16_t mask = 0x0001;
		for (int i = 0; i < NUM_BRICKS_PER_BMB; i++)
		{
			if (bmb[bmbIdx].balSwEnabled[i])
			{
				balanceSwEnabled |= mask;
			}
			mask = mask << 1;
		}
		// Update the balance switches on the relevant BMB
		writeDevice(BALSWEN, balanceSwEnabled, bmbIdx);
	}

}
