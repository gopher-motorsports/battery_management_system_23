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

#define WATCHDOG_1S_STEP_SIZE 							0x1000
#define WATCHDOG_TIMER_LOAD_5 							0x0500
#define DEVCFG1_ENABLE_ALIVE_COUNTER					0x0040
#define DEVCFG1_DEFAULT_CONFIG							0x1002
#define MEASUREEN_ENABLE_BRICK_CHANNELS 				0x0FFF
#define MEASUREEN_ENABLE_VBLOCK_CHANNEL 				0xC000
#define MEASUREEN_ENABLE_AIN1_CHANNEL					0x1000
#define MEASUREEN_ENABLE_AIN2_CHANNEL					0x2000
#define ACQCFG_THRM_ON									0x0300
#define ACQCFG_MAX_SETTLING_TIME						0x003F
#define AUTOBALSWDIS_5MS_RECOVERY_TIME					0x0034
#define SCANCTRL_START_SCAN								0x0001
#define SCANCTRL_4_OVERSAMPLES							0x0010
#define SCANCTRL_32_OVERSAMPLES							0x0040
#define SCANCTRL_ENABLE_AUTOBALSWDIS					0x0800
#define DIAGCFG_REF_VOLT_DIAGNOSTIC						0x0001
#define DIAGCFG_VAA_DIAGNOSTIC							0x0002
#define DIAGCFG_LSAMP_DIAGNOSTIC						0x0003
#define DIAGCFG_ZERO_ADC_DIAGNOSTIC						0x0004
#define DIAGCFG_FULL_ADC_DIAGNOSTIC						0x0005
#define DIAGCFG_DIE_TEMP_DIAGNOSTIC						0x0006
#define SCANCTRL_BALSW_SHORT_DIAGNOSTIC					0x0100
#define SCANCTRL_BALSW_OPEN_DIAGNOSTIC					0x0200
#define SCANCTRL_CELL_SENSE_OPEN_ODDS_DIAGNOSTIC		0x0500
#define SCANCTRL_CELL_SENSE_OPEN_EVENS_DIAGNOSTIC		0x0600
#define BALSWEN_CLOSE_ODD_SWITCHES						0x0555
#define BALSWEN_CLOSE_EVEN_SWITCHES						0x0AAA



/* ==================================================================== */
/* ========================= LOCAL VARIABLES ========================== */
/* ==================================================================== */
static uint8_t recvBuffer[SPI_BUFF_SIZE];


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */


/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

static bool scanComplete(uint32_t numBmbs);
static bool performAcquisition(Bmb_Mux_State_E muxState, Bmb_Diagnostic_State_E diagnosticState, uint32_t numBmbs);
static bool verifyBalswClosed(uint32_t numBmbs);
static bool verifyBalswOpen(uint32_t numBmbs);

static void updateCellData(Bmb_S* bmb, uint32_t numBmbs);
static void updateTempData(Bmb_S* bmb, Bmb_Mux_State_E muxState, uint32_t numBmbs);
static void updateDiagnosticData(Bmb_S* bmb, Bmb_Diagnostic_State_E diagnosticState, uint32_t numBmbs);
static void updateBalswDiagnosticData(Bmb_S* bmb, uint32_t numBmbs);

void updateBmbBalanceSwitches(Bmb_S* bmb);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

static bool scanComplete(uint32_t numBmbs)
{
	// Verify that Scan completed successfully
	if (readAll(SCANCTRL, recvBuffer, numBmbs))
	{
		bool allBmbScanDone = true;
		for (uint8_t j = 0; j < numBmbs; j++)
		{
			uint16_t scanCtrlData = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
			allBmbScanDone &= ((scanCtrlData & 0xA000) == 0xA000);	// Verify SCANDONE and DATARDY bits
		}
		if(!allBmbScanDone)
		{
			Debug("All BMB Scans failed to complete in time\n");
		}
		return allBmbScanDone;
	}
	else
	{
		// TODO - improve this. Handle failure correctly
		Debug("Failed to read SCANCTRL register\n");
		return false;
	}
}

static bool performAcquisition(Bmb_Mux_State_E muxState, Bmb_Diagnostic_State_E diagnosticState, uint32_t numBmbs)
{
	uint16_t gpio 		= 	(0xF000 | (muxState & 0x0007));
	uint16_t diagCfg 	= 	((diagnosticState + 1) & 0x0007);
	uint16_t scanCtrl 	= 	(SCANCTRL_START_SCAN | SCANCTRL_32_OVERSAMPLES | SCANCTRL_ENABLE_AUTOBALSWDIS);

	bool scanSuccess = true;

	scanSuccess &= writeAll(GPIO, gpio, numBmbs);
	scanSuccess &= writeAll(DIAGCFG, diagCfg, numBmbs);
	scanSuccess &= writeAll(SCANCTRL, scanCtrl, numBmbs);
	return scanSuccess;
}

static bool verifyBalswClosed(uint32_t numBmbs)
{
	return writeAll(SCANCTRL, SCANCTRL_START_SCAN | SCANCTRL_BALSW_OPEN_DIAGNOSTIC | SCANCTRL_4_OVERSAMPLES, numBmbs);
}

static bool verifyBalswOpen(uint32_t numBmbs)
{
	return writeAll(SCANCTRL, SCANCTRL_START_SCAN | SCANCTRL_BALSW_SHORT_DIAGNOSTIC | SCANCTRL_4_OVERSAMPLES, numBmbs);
}

static void updateCellData(Bmb_S* bmb, uint32_t numBmbs)
{
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
		Debug("Error during VBLOCK readAll!\n");

		// Failed to acquire data. Set status to MIA
		for (int j = 0; j < numBmbs; j++)
		{
			bmb[j].blockVStatus = MIA;
		}
	}
}

static void updateTempData(Bmb_S* bmb, Bmb_Mux_State_E muxState, uint32_t numBmbs)
{
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
			Debug("Error during TEMP readAll!\n");
			// Failed to acquire data. Set status to MIA
			for (int j = 0; j < numBmbs; j++)
			{
				bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = MIA;
			}
		}
	}
}

static void updateDiagnosticData(Bmb_S* bmb, Bmb_Diagnostic_State_E diagnosticState, uint32_t numBmbs)
{
	switch (diagnosticState)
	{
	case BMB_REFERENCE_VOLTAGE_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
				bmb[j].fault[BMB_REFERENCE_VOLTAGE_F] = ((diagRaw < ALTREF_MIN) || (diagRaw > ALTREF_MAX));
			}
		}
		else
		{
			Debug("Error during bmb reference voltage diagnostic update!\n");
		}
		break;
	case BMB_VAA_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float vaa = diagRaw * CONVERT_14BIT_TO_5V;
				bmb[j].fault[BMB_VAA_F] = ((vaa < VAA_MIN) || (vaa > VAA_MAX));
			}

		}
		else
		{
			Debug("Error during bmb VAA diagnostic update!\n");
		}
		break;
	case BMB_LSAMP_OFFSET_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				diagRaw = (diagRaw >= 0x2000) ? (diagRaw - 0x2000) : (0x2000 - diagRaw);
				float lsampOffset = diagRaw * CONVERT_14BIT_TO_5V;
				bmb[j].fault[BMB_LSAMP_OFFSET_F] = (lsampOffset > LSAMP_MAX_OFFSET);
			}
		}
		else
		{
			Debug("Error during bmb LSAMP Offset diagnostic update!\n");
		}
		break;
	case BMB_ADC_BIT_STUCK_HIGH_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
				bmb[j].fault[BMB_ADC_BIT_STUCK_HIGH_F] = (diagRaw != ZERO_SCALE_ADC_SUCCESS);
			}
		}
		else
		{
			Debug("Error during bmb ADC bit stuck high diagnostic update!\n");
		}
		break;
	case BMB_ADC_BIT_STUCK_LOW_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
				bmb[j].fault[BMB_ADC_BIT_STUCK_LOW_F] = (diagRaw != FULL_SCALE_ADC_SUCCESS);
			}
		}
		else
		{
			Debug("Error during bmb ADC bit stuck low diagnostic update!\n");
		}
		break;
	case BMB_DIE_TEMP_DIAGNOSTIC:
		if (readAll(DIAG, recvBuffer, numBmbs))
		{
			for(uint32_t j = 0; j < numBmbs; j++)
			{
				uint16_t diagRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
				float dieTempM = diagRaw * CONVERT_14BIT_TO_VREF;
				dieTempM *= CONVERT_DIE_TEMP_GAIN;
				dieTempM += CONVERT_DIE_TEMP_OFFSET;
				bmb[j].dieTemp = dieTempM;
				bmb[j].fault[BMB_DIE_TEMP_F] = (dieTempM > DIE_TEMP_MAX);
			}
		}
		else
		{
			Debug("Error during bmb die temperature diagnostic update!\n");
		}
		break;
	default:
		break;
	}
}

static void updateBalswDiagnosticData(Bmb_S* bmb, uint32_t numBmbs)
{
	if (readAll(ALRTBALSW, recvBuffer, numBmbs))
	{
		for(uint32_t j = 0; j < numBmbs; j++)
		{
			uint16_t alrtBalSw = (recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j];
			bool errorPresent = false;
			for(uint8_t i = 1; i <= NUM_BRICKS_PER_BMB; i++)
			{
				if(alrtBalSw & 0x0001)
				{
					Debug("BMB balance switch error at SW: %d\n", i);
					errorPresent = true;
				}
				alrtBalSw = alrtBalSw >> 1;
			}
			bmb[j].fault[BMB_BAL_SW_F] = errorPresent;
		}
	}
	else
	{
		Debug("Error during BMB BALSW diagnostic update!\n");
	}
}

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
  @param   bmb - The array containing BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregateBmbData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Iterate through brick voltages
	for (int i = 0; i < numBmbs; i++)
	{
		Bmb_S* pBmb = &bmb[i];
		float maxBrickV = MIN_VOLTAGE_SENSOR_VALUE_V;
		float minBrickV = MAX_VOLTAGE_SENSOR_VALUE_V;
		float stackV	= 0.0f;
		float maxBrickTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBrickTemp = MAX_TEMP_SENSOR_VALUE_C;
		float brickTempSum = 0.0f;
		float maxBoardTemp = MIN_TEMP_SENSOR_VALUE_C;
		float minBoardTemp = MAX_TEMP_SENSOR_VALUE_C;
		float boardTempSum = 0.0f;
		// TODO If SNA do not count
		// Aggregate brick voltage and temperature data
		for (int j = 0; j < NUM_BRICKS_PER_BMB; j++)
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

			stackV += brickV;
			brickTempSum += brickTemp;
		}

		// Aggregate board temp data
		for (int j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
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
		pBmb->stackV	= stackV;
		pBmb->avgBrickV = stackV / NUM_BRICKS_PER_BMB;
		pBmb->maxBrickTemp = maxBrickTemp;
		pBmb->minBrickTemp = minBrickTemp;
		pBmb->avgBrickTemp = brickTempSum / NUM_BRICKS_PER_BMB;
		pBmb->maxBoardTemp = maxBoardTemp;
		pBmb->minBoardTemp = minBoardTemp;
		pBmb->avgBoardTemp = boardTempSum / NUM_BOARD_TEMP_PER_BMB;
	}
}

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
	for (int i = 0; i < NUM_BRICKS_PER_BMB; i++)
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
bool initBmbs(uint32_t numBmbs)
{
	// bool initialized = true;
	// uint32_t bmbsDetected = 0;

	// Debug("Initializing ASCI connection...\n");
	// resetASCI();
	// initialized &= initASCI();
	// initialized &= helloAll(&bmbsDetected);

	// if(bmbsDetected != numBmbs)
	// {
	// 	Debug("Number of BMBs detected (%lu) doesn't match expectation (%lu)\n", bmbsDetected, numBmbs);
	// 	initialized = false;
	// }
	// else
	// {
	// 	printf("Number of BMBs detected: %lu\n", bmbsDetected);
	// }

	// // If the ASCI failed to init, return false and do not attempt to write to bmbs
	// if(!initialized)
	// {
	// 	Debug("Failed to Initialize ASCI connection...\n");
	// 	return false;
	// }
	
	// TODO - do we want to read the register contents back and verify values?
	// Enable alive counter byte
	// numBmbs set to 0 since alive counter not yet enabled
	writeAll(DEVCFG1, (DEVCFG1_DEFAULT_CONFIG | DEVCFG1_ENABLE_ALIVE_COUNTER), numBmbs);

	// Enable measurement channels
	writeAll(MEASUREEN, (MEASUREEN_ENABLE_BRICK_CHANNELS | MEASUREEN_ENABLE_VBLOCK_CHANNEL | MEASUREEN_ENABLE_AIN1_CHANNEL | MEASUREEN_ENABLE_AIN2_CHANNEL), numBmbs);

	// Manual set THRM HIGH and config settling time
	writeAll(ACQCFG, (ACQCFG_THRM_ON | ACQCFG_MAX_SETTLING_TIME), numBmbs);

	// Enable 5ms delay between balancing and aquisition
	writeAll(AUTOBALSWDIS, AUTOBALSWDIS_5MS_RECOVERY_TIME, numBmbs);

	// Satan gave me this number - (2v * (2^14)/(5v)) << 2
	writeAll(BALSHRTTHR, 0x6666, numBmbs);
	// ((1mv * (2^14)/(5v)) + 2^13) << 2
	// writeAll(BALLOWTHR, 0x800D, numBmbs);
	writeAll(BALLOWTHR, 32794, numBmbs);

	// ((20mv * (2^14)/(5v)) + 2^13) << 2
	writeAll(BALHIGHTHR, 0x8106, numBmbs);
	// Reset MUX configuration to Channel 1 - 000

	// Start initial acquisition with 32 oversamples
	performAcquisition(0, 0, numBmbs);

	// Set brickOV voltage alert threshold
	// Set brickUV voltage alert threshold
}

void runBmbUpdate(Bmb_S* bmb, uint32_t numBmbs)
{
	static Bmb_Scan_State_E currState = SCAN_STACK;
	Bmb_Scan_State_E nextState = SCAN_STACK;

	static Bmb_Diagnostic_State_E diagnosticState = BMB_REFERENCE_VOLTAGE_DIAGNOSTIC;
	static Bmb_Mux_State_E muxState = MUX1;

	switch (currState)
	{
	case SCAN_STACK:
		static uint32_t lastScan = 0;
		if(HAL_GetTick() - lastScan >= 50)
		{
			lastScan = HAL_GetTick();
			if(scanComplete(numBmbs))
			{
				updateCellData(bmb, numBmbs);
				updateTempData(bmb, muxState, numBmbs);
				aggregateBmbData(bmb, numBmbs);
				updateDiagnosticData(bmb, diagnosticState, numBmbs);

				// Cycle to next MUX configuration
				muxState = (muxState + 1) % NUM_MUX_CHANNELS;

				// Cycle to next diagnostic configuration
				diagnosticState = (diagnosticState + 1) % NUM_BMB_DIAGNOSTIC_STATES;

				if(verifyBalswClosed(numBmbs))
				{
					nextState = VERIFY_BALSW_CLOSED;
				}
				else
				{
					nextState = SCAN_ERROR;
				}

			}
			else
			{
				for(uint8_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
				{
					for(uint8_t j = 0; j < numBmbs; j++)
					{
						bmb[j].brickVStatus[i] = MIA;
					}
				}
				nextState = SCAN_ERROR;
			}
		}
		break;
	case VERIFY_BALSW_CLOSED:
		if(scanComplete(numBmbs))
		{
			updateBalswDiagnosticData(bmb, numBmbs);
		
			if(verifyBalswOpen(numBmbs))
			{
				nextState = VERIFY_BALSW_OPEN;
			}
			else
			{
				nextState = SCAN_ERROR;
			}
		}
		else
		{
			nextState = SCAN_ERROR;
		}
		break;
	case VERIFY_BALSW_OPEN:
		if(scanComplete(numBmbs))
		{
			updateBalswDiagnosticData(bmb, numBmbs);

			updateBmbBalanceSwitches(bmb);
		
			if(performAcquisition(muxState, diagnosticState, numBmbs))
			{
				nextState = SCAN_STACK;
			}
			else
			{
				nextState = SCAN_ERROR;
			}
		}
		else
		{
			nextState = SCAN_ERROR;
		}
		break;
	case SCAN_ERROR:
		if(performAcquisition(muxState, diagnosticState, numBmbs))
		{
			nextState = SCAN_STACK;
		}
		else
		{
			nextState = SCAN_ERROR;
		}
		break;
	default:
		nextState = SCAN_ERROR;
		break;
	}
	currState = nextState;
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
	for (int bmbIdx = 0; bmbIdx < numBmbs; bmbIdx++)
	{
		uint32_t numBricksNeedBalancing = 0;
		Brick_S bricksToBalance[NUM_BRICKS_PER_BMB];
		// Add all bricks that need balancing to array if board temp allows for it
		if (bmb[bmbIdx].maxBoardTemp < MAX_BOARD_TEMP_BALANCING_ALLOWED_C)
		{
			for (int brickIdx = 0; brickIdx < NUM_BRICKS_PER_BMB; brickIdx++)
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
		// Update the BMB balance switches in hardware
		updateBmbBalanceSwitches(&bmb[bmbIdx]);
	}
}
