/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "cmsis_os.h"
#include "alerts.h"
#include "bms.h"
#include "bmb.h"
#include "bmbInterface.h"
#include "leakyBucket.h"
#include "debug.h"
#include "GopherCAN.h"
#include "currentSense.h"
#include "internalResistance.h"
#include "gopher_sense.h"
#include "charger.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */
#define MAX_BRICK_VOLTAGE_READING 5.0f
#define MIN_BRICK_VOLTAGE_READING 0.0f

#define MAX_TEMP_SENSE_READING 	  120.0f
#define MIN_TEMP_SENSE_READING	  (-40.0f)

#define EPAP_UPDATE_PERIOD_MS	  2000
#define ALERT_MONITOR_PERIOD_MS	  10

Bms_S gBms = 
{
    .numBmbs = NUM_BMBS_IN_ACCUMULATOR,
    .bmb = 
	{
        [0] = {.bmbIdx = 0},
        [1] = {.bmbIdx = 1},
        [2] = {.bmbIdx = 2},
        [3] = {.bmbIdx = 3},
        [4] = {.bmbIdx = 4},
        [5] = {.bmbIdx = 5},
        [6] = {.bmbIdx = 6}
    },
	/* Initially we can assume that the SOC by OCV method is reliable since pack was just initialized*/
	.soc.socByOcvGoodTimer.timCount = SOC_BY_OCV_GOOD_QUALIFICATION_TIME_MS, 
	.soc.socByOcvGoodTimer.lastUpdate = 0,
	.soc.socByOcvGoodTimer.timThreshold = SOC_BY_OCV_GOOD_QUALIFICATION_TIME_MS
};


/* ==================================================================== */
/* ======================= EXTERNAL VARIABLES ========================= */
/* ==================================================================== */

extern osMessageQId epaperQueueHandle;
extern CAN_HandleTypeDef hcan2;

extern bool newChargerMessage;
extern LeakyBucket_S asciCommsLeakyBucket;

/* ==================================================================== */
/* =================== LOCAL FUNCTION DECLARATIONS ==================== */
/* ==================================================================== */

static void disableBmbBalancing(Bmb_S* bmb);


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

static void disableBmbBalancing(Bmb_S* bmb)
{
	for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
	{
		bmb->balSwRequested[i] = false;
	}
}

static void setAmsFault(bool set)
{
	// AMS fault pin is active low so if set == true then pin should be low
	HAL_GPIO_WritePin(AMS_FAULT_OUT_GPIO_Port, AMS_FAULT_OUT_Pin, set ? GPIO_PIN_RESET : GPIO_PIN_SET);
	return;
}


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void initBmsGopherCan(CAN_HandleTypeDef* hcan)
{
	// initialize CAN
	if (init_can(GCAN0, hcan, BMS_ID, BXTYPE_MASTER))
	{
		gBms.bmsHwState = BMS_GSNS_FAILURE;
	}
}

/*!
  @brief   Initialization function for the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @returns bool True if initialization successful, false otherwise
*/
bool initBatteryPack(uint32_t* numBmbs)
{
	setAmsFault(true);
	gBms.balancingDisabled = true;
	gBms.emergencyBleed    = false;
	gBms.chargingDisabled  = true;
	gBms.limpModeEnabled   = false;
	gBms.amsFaultPresent   = false;
	
	if (!initASCI())
	{
		goto initializationError;
	}			
	helloAll(numBmbs);	// Ignore return value as it will be bad due to no loopback
	
	// Set internal loopback on final BMB
	if (!setBmbInternalLoopback(NUM_BMBS_IN_ACCUMULATOR - 1, true))
	{
		goto initializationError;
	}

	if (helloAll(numBmbs))
	{
		// helloAll command succeeded - verify that the numBmbs was correctly set
		if (*numBmbs != NUM_BMBS_IN_ACCUMULATOR)
		{
			Debug("Number of BMBs detected (%lu) doesn't match expectation (%d)\n", *numBmbs, NUM_BMBS_IN_ACCUMULATOR);
			goto initializationError;
		}

		initBmbs(*numBmbs);
		gBms.numBmbs = *numBmbs;
		gBms.bmsHwState = BMS_NOMINAL;
		setAmsFault(false);
		// Leaky bucket was filled due to missing external loopback. Since we successfully initialized using
		// internal loopback, we can reset the leaky bucket
		resetLeakyBucket(&asciCommsLeakyBucket);
		return true;

	}
	else
	{
		goto initializationError;
	}
	
// Routine if initialization error ocurs
initializationError:
	// Set hardware error status
	gBms.bmsHwState = BMS_BMB_FAILURE;

	// Determine if a chain break exists
	initASCI();
	helloAll(numBmbs);	// Ignore return value as it will be bad due to no loopback
	uint32_t breakLocation = detectBmbDaisyChainBreak(gBms.bmb, NUM_BMBS_IN_ACCUMULATOR);
	if (breakLocation != 0)
	{
		// A chain break exists
		if (breakLocation == 1)
		{
			Debug("BMB Chain Break detected between BMS and BMB 1\n");
		}
		else
		{
			Debug("BMB Chain Break detected between BMB %lu and BMB %lu\n", breakLocation - 1, breakLocation);
		}
	}
	else
	{
		// Break location is in the external loopback of the final BMB
		// This is fixable by enabling the internal loopback on the final BMB which is 
		// done in the detectBmbDaisyChainBreak function
		if (helloAll(numBmbs))
		{
			// helloAll command succeeded - verify that the numBmbs was correctly set
			if (*numBmbs != NUM_BMBS_IN_ACCUMULATOR)
			{
				return false;
			}
			initBmbs(*numBmbs);
			gBms.numBmbs = *numBmbs;
			gBms.bmsHwState = BMS_NOMINAL;
			setAmsFault(false);
			// Leaky bucket was filled due to missing external loopback. Since we successfully initialized using
			// internal loopback, we can reset the leaky bucket
			resetLeakyBucket(&asciCommsLeakyBucket);
			return true;
		}
	}
	return false;
}

/*!
  @brief   Updates all BMB data
  @param   numBmbs - The expected number of BMBs in the daisy chain\
*/
void updatePackData(uint32_t numBmbs)
{
	static uint32_t lastPackUpdate = 0;
	if(HAL_GetTick() - lastPackUpdate > VOLTAGE_DATA_UPDATE_PERIOD_MS)
	{
		updateBmbData(gBms.bmb, numBmbs);
		// // TODO: Get rid of this
		// for (int i = 0; i < 12; i++)
		// {
		// 	gBms.bmb[0].brickV[i] = 3.7f;
		// 	gBms.bmb[0].brickVStatus[i] = GOOD;
		// 	gBms.bmb[0].brickTemp[i] = 25.0f;
		// 	gBms.bmb[0].brickTempStatus[i] = GOOD;
		// }
		// for (int i = 0; i < 4; i++)
		// {
		// 	gBms.bmb[0].boardTemp[i] = 30.0f;
		// 	gBms.bmb[0].boardTempStatus[i] = GOOD;
		// }
		// // TODO: Get rid of this ^
		aggregatePackData(numBmbs);
		updateInternalResistanceCalcs(&gBms);
	}
}

/*!
  @brief   Handles balancing the battery pack
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @param   balanceRequested - True if we want to balance, false otherwise
*/
void balancePack(uint32_t numBmbs, bool balanceRequested)
{
	// TODO: Determine how we want to handle EMERGENCY_BLEED

	// If balancing not requested or balancing disabled ensure all balance switches off
	if (!balanceRequested || gBms.balancingDisabled)
	{
		for (int32_t i = 0; i < numBmbs; i++)
		{
			disableBmbBalancing(&gBms.bmb[i]);
		}
		balanceCells(gBms.bmb, numBmbs);
		return;
	}

	// Determine minimum voltage across entire battery pack
	float bleedTargetVoltage = gBms.minBrickV;

	// Ensure we don't overbleed the cells
	if (bleedTargetVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		bleedTargetVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}		
	balancePackToVoltage(numBmbs, bleedTargetVoltage);
}

/*!
  @brief   Balance the battery pack to a specified target brick voltage.
  @param   numBmbs - The number of Battery Management Boards (BMBs) in the pack.
  @param   targetBrickVoltage - The target voltage for each brick in the pack.

  This function balances the battery pack by setting the bleed request for each brick
  in every BMB based on the target brick voltage. The target brick voltage is clamped
  to a minimum value (MIN_BLEED_TARGET_VOLTAGE_V) if it's too low. The function iterates
  through all BMBs and bricks, checking whether they should be bled or not by comparing
  the brick voltage to the target voltage plus a balance threshold (BALANCE_THRESHOLD_V).
  Finally, the balanceCells() function is called to apply the bleed requests to the cells.
*/
void balancePackToVoltage(uint32_t numBmbs, float targetBrickVoltage)
{
	// Clamp target brick voltage if too low
	if (targetBrickVoltage < MIN_BLEED_TARGET_VOLTAGE_V)
	{
		targetBrickVoltage = MIN_BLEED_TARGET_VOLTAGE_V;
	}

	// Iterate through all BMBs and set bleed request
	for (int32_t i = 0; i < numBmbs; i++)
	{
		// Iterate through all bricks and determine whether they should be bled or not
		for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
		{
			if (gBms.bmb[i].brickV[j] > targetBrickVoltage + BALANCE_THRESHOLD_V)
			{
				gBms.bmb[i].balSwRequested[j] = true;
			}
			else
			{
				gBms.bmb[i].balSwRequested[j] = false;
			}
		}
	}
	
	balanceCells(gBms.bmb, numBmbs);
}

/*!
  @brief   Check and handle alerts for the BMS by running alert monitors, accumulating alert statuses,
           and setting BMS status based on the alerts.
  
  This function runs each alert monitor, checks the status of each alert, and sets the BMS status
  based on the alert responses. If an alert is set, it iterates through all alert responses and
  activates the corresponding response in the BMS. The BMS status is then updated accordingly.
*/
void checkAndHandleAlerts()
{
	static uint32_t lastAlertMonitorUpdate = 0;

	if (HAL_GetTick() - lastAlertMonitorUpdate > ALERT_MONITOR_PERIOD_MS)
	{
		// Run each alert monitor
		for (uint32_t i = 0; i < NUM_ALERTS; i++)
		{
			runAlertMonitor(&gBms, alerts[i]);
		}

		// Accumulate alert statuses
		bool responseStatus[NUM_ALERT_RESPONSES] = { false };

		// Check each alert status
		for (uint32_t i = 0; i < NUM_ALERTS; i++)
		{
			Alert_S* alert = alerts[i];
			if (getAlertStatus(alert) == ALERT_SET)
			{
				// Iterate through all alert responses and set them
				for (uint32_t j = 0; j < alert->numAlertResponse; j++)
				{
					const AlertResponse_E response = alert->alertResponse[j];
					// Set the alert response to active
					responseStatus[response] = true;
				}
			}
		}

		// Set BMS status based on alert
		gBms.balancingDisabled = responseStatus[DISABLE_BALANCING];
		gBms.emergencyBleed	   = responseStatus[EMERGENCY_BLEED];
		gBms.chargingDisabled  = responseStatus[DISABLE_CHARGING];
		gBms.limpModeEnabled   = responseStatus[LIMP_MODE];
		gBms.amsFaultPresent   = responseStatus[AMS_FAULT];
		setAmsFault(gBms.amsFaultPresent);
	}
	
}

/*!
  @brief   Update BMS data statistics. Min/Max/Avg
  @param   numBmbs - The expected number of BMBs in the daisy chain
*/
void aggregatePackData(uint32_t numBmbs)
{
	Bms_S* pBms = &gBms;
	// Update BMB level stats
	aggregateBmbData(pBms->bmb, numBmbs);

	float maxBrickV	   = MIN_BRICK_VOLTAGE_READING;
	float minBrickV	   = MAX_BRICK_VOLTAGE_READING;
	float avgBrickVSum = 0.0f;
	float accumulatorVSum = 0.0f;

	float maxBrickTemp    = MIN_TEMP_SENSE_READING;
	float minBrickTemp 	  = MAX_TEMP_SENSE_READING;
	float avgBrickTempSum = 0.0f;

	float maxBoardTemp    = MIN_TEMP_SENSE_READING;
	float minBoardTemp 	  = MAX_TEMP_SENSE_READING;
	float avgBoardTempSum = 0.0f;

	for (int32_t i = 0; i < numBmbs; i++)
	{
		Bmb_S* pBmb = &pBms->bmb[i];

		if (pBmb->maxBrickV > maxBrickV)
		{
			maxBrickV = pBmb->maxBrickV;
		}
		if (pBmb->minBrickV < minBrickV)
		{
			minBrickV = pBmb->minBrickV;
		}

		if (pBmb->maxBrickTemp > maxBrickTemp)
		{
			maxBrickTemp = pBmb->maxBrickTemp;
		}
		if (pBmb->minBrickTemp < minBrickTemp)
		{
			minBrickTemp = pBmb->minBrickTemp;
		}

		if (pBmb->maxBoardTemp > maxBoardTemp)
		{
			maxBoardTemp = pBmb->maxBoardTemp;
		}
		if (pBmb->minBoardTemp < minBoardTemp)
		{
			minBoardTemp = pBmb->minBoardTemp;
		}

		avgBrickVSum += pBmb->avgBrickV;
		accumulatorVSum += pBmb->sumBrickV;
		avgBrickTempSum += pBmb->avgBrickTemp;
		avgBoardTempSum += pBmb->avgBoardTemp;
	}
	pBms->accumulatorVoltage = accumulatorVSum;
	pBms->maxBrickV = maxBrickV;
	pBms->minBrickV = minBrickV;
	pBms->avgBrickV = avgBrickVSum / NUM_BMBS_IN_ACCUMULATOR;
	pBms->maxBrickTemp = maxBrickTemp;
	pBms->minBrickTemp = minBrickTemp;
	pBms->avgBrickTemp = avgBrickTempSum / NUM_BMBS_IN_ACCUMULATOR;
	pBms->maxBoardTemp = maxBoardTemp;
	pBms->minBoardTemp = minBoardTemp;
	pBms->avgBoardTemp = avgBoardTempSum / NUM_BMBS_IN_ACCUMULATOR;
}

/*!
  @brief   Update the IMD status based on measured frequency and duty cycle
*/
void updateImdStatus()
{
	Bms_S* pBms = &gBms;

	pBms->imdState = getImdStatus();
}

/*!
  @brief   Update the epaper display with current data
*/
void updateEpaper()
{
	static uint32_t lastEpapUpdate = 0;

	if((HAL_GetTick() - lastEpapUpdate) > EPAP_UPDATE_PERIOD_MS)
	{
		lastEpapUpdate = HAL_GetTick();

		Epaper_Data_S epapData;
		epapData.avgBrickV = gBms.avgBrickV;
		epapData.maxBrickV = gBms.maxBrickV;
		epapData.minBrickV = gBms.minBrickV;

		epapData.avgBrickTemp = gBms.avgBrickTemp;
		epapData.maxBrickTemp = gBms.maxBrickTemp;
		epapData.minBrickTemp = gBms.minBrickTemp;

		epapData.avgBoardTemp = gBms.avgBoardTemp;
		epapData.maxBoardTemp = gBms.maxBoardTemp;
		epapData.minBoardTemp = gBms.minBoardTemp;

		epapData.current = gBms.tractiveSystemCurrent;

		epapData.stateOfEnergy = gBms.soc.soeByOcv;

		// Send the current state of the BMS state machine
		epapData.stateMessage = "TEMP STATE";

		// Active Alert Cycling
		static uint32_t currAlertMessageIndex = 0;	// Holds the index of the alert array that is currently being displayed
		uint32_t numAlertsSet = 0;				// Holds the number of alerts currently active
		uint32_t indexNextAlert = 0;			// Used to find the index of an active alert greater than the index of the currently displayed alert
		uint32_t indexMinAlert = 0;				// Used to find the index of the first active alert regardless of the currently displayed alert
		bool nextAlertFound = false;			// Set to true if an alert with a higher index than the currently set alert is active
		bool minAlertFound = false;				// Set to true once/if the smallest index active alert is set

		// Cycle through all alerts
		for (uint32_t i = 0; i < NUM_ALERTS; i++)
		{
			Alert_S* alert = alerts[i];

			// Triggers only if alert is active
			if (getAlertStatus(alert) == ALERT_SET)
			{
				// Increment active alert counter
				numAlertsSet++;

				// The first active alert found will be used for indexMinAlert, and will not be set again
				if(!minAlertFound)
				{
					minAlertFound = true;
					indexMinAlert = i;
				}

				// The first active alert greater than the curretly displayed active alert index will be used for currAlertMessageIndex
				// and will not be set again
				if(!nextAlertFound)
				{
					indexNextAlert++;
					if(i > currAlertMessageIndex)
					{
						nextAlertFound = true;
						currAlertMessageIndex = i;
					}
				}
			}
		}

		// Update epaper data struct with the number of active alerts
		epapData.numActiveAlerts = numAlertsSet;

		// Only update the epaper struct data if there are active alerts
		// If there are no alerts, the epaper will ignore what is currently set in the currAlertIndex and alertMessage variables
		if(numAlertsSet > 0)
		{
			// If there is an active alert with index greater than the currently displayed alert, that alert is sent to the epaper
			// Otherwise, the alert index is reset to 1 and the min alert is sent to the epaper
			if(nextAlertFound)
			{
				epapData.currAlertIndex = indexNextAlert;
				epapData.alertMessage = (char*)alerts[currAlertMessageIndex]->alertName;
			}
			else
			{
				currAlertMessageIndex = indexMinAlert;
				epapData.currAlertIndex = 1;
				epapData.alertMessage = (char*)alerts[indexMinAlert]->alertName;
			}
		}

		// Send epaper Data in queue to epaper
		xQueueOverwrite(epaperQueueHandle, &epapData);
	}
}

/*!
  @brief   Update the tractive system current
*/
void updateTractiveCurrent()
{
	static uint32_t lastCurrentUpdate = 0;
	if((HAL_GetTick() - lastCurrentUpdate) >= CURRENT_SENSOR_UPDATE_PERIOD_MS)
	{
		lastCurrentUpdate = HAL_GetTick();
		getTractiveSystemCurrent(&gBms);
	}	
}

/*!
  @brief   Update the state of charge (SOC) and state of energy (SOE) using both SOC by 
		   Open Cell Voltage (OCV) and Coulomb Counting
*/
void updateStateOfChargeAndEnergy()
{
	static uint32_t lastSocAndSoeUpdate = 0;
	if ((HAL_GetTick() - lastSocAndSoeUpdate) >= SOC_AND_SOE_UPDATE_PERIOD_MS)
	{
		uint32_t deltaTimeMs = HAL_GetTick() - lastSocAndSoeUpdate;
		lastSocAndSoeUpdate = HAL_GetTick();

		// Populate data to be used in SOC and SOE calculation
		Soc_S* pSoc = &gBms.soc;
		pSoc->minBrickVoltage = gBms.minBrickV;
		pSoc->curAccumulatorCurrent = gBms.tractiveSystemCurrent;
		pSoc->deltaTimeMs = deltaTimeMs;
		updateSocAndSoe(&gBms.soc);
	}	
}

/*!
  @brief   Log non-ADC gopher can variables
*/
void updateGopherCan()
{
	// This condition is to stop gcan sending if we are testing with less than the full accumulator
	// Otherwise, it will try to reference bmb indices that dont exist
	if((NUM_BMBS_IN_ACCUMULATOR == 7) && (NUM_BRICKS_PER_BMB == 12))
	{
		static FLOAT_CAN_STRUCT *cellVoltageParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1Cell1Voltage_V, &seg1Cell2Voltage_V, &seg1Cell3Voltage_V, &seg1Cell4Voltage_V, &seg1Cell5Voltage_V, &seg1Cell6Voltage_V, &seg1Cell7Voltage_V, &seg1Cell8Voltage_V, &seg1Cell9Voltage_V, &seg1Cell10Voltage_V, &seg1Cell11Voltage_V, &seg1Cell12Voltage_V},
			{&seg2Cell1Voltage_V, &seg2Cell2Voltage_V, &seg2Cell3Voltage_V, &seg2Cell4Voltage_V, &seg2Cell5Voltage_V, &seg2Cell6Voltage_V, &seg2Cell7Voltage_V, &seg2Cell8Voltage_V, &seg2Cell9Voltage_V, &seg2Cell10Voltage_V, &seg2Cell11Voltage_V, &seg2Cell12Voltage_V},
			{&seg3Cell1Voltage_V, &seg3Cell2Voltage_V, &seg3Cell3Voltage_V, &seg3Cell4Voltage_V, &seg3Cell5Voltage_V, &seg3Cell6Voltage_V, &seg3Cell7Voltage_V, &seg3Cell8Voltage_V, &seg3Cell9Voltage_V, &seg3Cell10Voltage_V, &seg3Cell11Voltage_V, &seg3Cell12Voltage_V},
			{&seg4Cell1Voltage_V, &seg4Cell2Voltage_V, &seg4Cell3Voltage_V, &seg4Cell4Voltage_V, &seg4Cell5Voltage_V, &seg4Cell6Voltage_V, &seg4Cell7Voltage_V, &seg4Cell8Voltage_V, &seg4Cell9Voltage_V, &seg4Cell10Voltage_V, &seg4Cell11Voltage_V, &seg4Cell12Voltage_V},
			{&seg5Cell1Voltage_V, &seg5Cell2Voltage_V, &seg5Cell3Voltage_V, &seg5Cell4Voltage_V, &seg5Cell5Voltage_V, &seg5Cell6Voltage_V, &seg5Cell7Voltage_V, &seg5Cell8Voltage_V, &seg5Cell9Voltage_V, &seg5Cell10Voltage_V, &seg5Cell11Voltage_V, &seg5Cell12Voltage_V},
			{&seg6Cell1Voltage_V, &seg6Cell2Voltage_V, &seg6Cell3Voltage_V, &seg6Cell4Voltage_V, &seg6Cell5Voltage_V, &seg6Cell6Voltage_V, &seg6Cell7Voltage_V, &seg6Cell8Voltage_V, &seg6Cell9Voltage_V, &seg6Cell10Voltage_V, &seg6Cell11Voltage_V, &seg6Cell12Voltage_V},
			{&seg7Cell1Voltage_V, &seg7Cell2Voltage_V, &seg7Cell3Voltage_V, &seg7Cell4Voltage_V, &seg7Cell5Voltage_V, &seg7Cell6Voltage_V, &seg7Cell7Voltage_V, &seg7Cell8Voltage_V, &seg7Cell9Voltage_V, &seg7Cell10Voltage_V, &seg7Cell11Voltage_V, &seg7Cell12Voltage_V}
		};

		static FLOAT_CAN_STRUCT *cellTempParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1Cell1Temp_C, &seg1Cell2Temp_C, &seg1Cell3Temp_C, &seg1Cell4Temp_C, &seg1Cell5Temp_C, &seg1Cell6Temp_C, &seg1Cell7Temp_C, &seg1Cell8Temp_C, &seg1Cell9Temp_C, &seg1Cell10Temp_C, &seg1Cell11Temp_C, &seg1Cell12Temp_C},
			{&seg2Cell1Temp_C, &seg2Cell2Temp_C, &seg2Cell3Temp_C, &seg2Cell4Temp_C, &seg2Cell5Temp_C, &seg2Cell6Temp_C, &seg2Cell7Temp_C, &seg2Cell8Temp_C, &seg2Cell9Temp_C, &seg2Cell10Temp_C, &seg2Cell11Temp_C, &seg2Cell12Temp_C},
			{&seg3Cell1Temp_C, &seg3Cell2Temp_C, &seg3Cell3Temp_C, &seg3Cell4Temp_C, &seg3Cell5Temp_C, &seg3Cell6Temp_C, &seg3Cell7Temp_C, &seg3Cell8Temp_C, &seg3Cell9Temp_C, &seg3Cell10Temp_C, &seg3Cell11Temp_C, &seg3Cell12Temp_C},
			{&seg4Cell1Temp_C, &seg4Cell2Temp_C, &seg4Cell3Temp_C, &seg4Cell4Temp_C, &seg4Cell5Temp_C, &seg4Cell6Temp_C, &seg4Cell7Temp_C, &seg4Cell8Temp_C, &seg4Cell9Temp_C, &seg4Cell10Temp_C, &seg4Cell11Temp_C, &seg4Cell12Temp_C},
			{&seg5Cell1Temp_C, &seg5Cell2Temp_C, &seg5Cell3Temp_C, &seg5Cell4Temp_C, &seg5Cell5Temp_C, &seg5Cell6Temp_C, &seg5Cell7Temp_C, &seg5Cell8Temp_C, &seg5Cell9Temp_C, &seg5Cell10Temp_C, &seg5Cell11Temp_C, &seg5Cell12Temp_C},
			{&seg6Cell1Temp_C, &seg6Cell2Temp_C, &seg6Cell3Temp_C, &seg6Cell4Temp_C, &seg6Cell5Temp_C, &seg6Cell6Temp_C, &seg6Cell7Temp_C, &seg6Cell8Temp_C, &seg6Cell9Temp_C, &seg6Cell10Temp_C, &seg6Cell11Temp_C, &seg6Cell12Temp_C},
			{&seg7Cell1Temp_C, &seg7Cell2Temp_C, &seg7Cell3Temp_C, &seg7Cell4Temp_C, &seg7Cell5Temp_C, &seg7Cell6Temp_C, &seg7Cell7Temp_C, &seg7Cell8Temp_C, &seg7Cell9Temp_C, &seg7Cell10Temp_C, &seg7Cell11Temp_C, &seg7Cell12Temp_C}
		};

		static FLOAT_CAN_STRUCT *boardTempParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1BMBBoardTemp1_C, &seg1BMBBoardTemp2_C, &seg1BMBBoardTemp3_C, &seg1BMBBoardTemp4_C},
			{&seg2BMBBoardTemp1_C, &seg2BMBBoardTemp2_C, &seg2BMBBoardTemp3_C, &seg2BMBBoardTemp4_C},
			{&seg3BMBBoardTemp1_C, &seg3BMBBoardTemp2_C, &seg3BMBBoardTemp3_C, &seg3BMBBoardTemp4_C},
			{&seg4BMBBoardTemp1_C, &seg4BMBBoardTemp2_C, &seg4BMBBoardTemp3_C, &seg4BMBBoardTemp4_C},
			{&seg5BMBBoardTemp1_C, &seg5BMBBoardTemp2_C, &seg5BMBBoardTemp3_C, &seg5BMBBoardTemp4_C},
			{&seg6BMBBoardTemp1_C, &seg6BMBBoardTemp2_C, &seg6BMBBoardTemp3_C, &seg6BMBBoardTemp4_C},
			{&seg7BMBBoardTemp1_C, &seg7BMBBoardTemp2_C, &seg7BMBBoardTemp3_C, &seg7BMBBoardTemp4_C}
		};

		static FLOAT_CAN_STRUCT *cellVoltageStatsParams[NUM_BMBS_IN_ACCUMULATOR][4] =
		{
			{&seg1Voltage_V, &seg1AveCellVoltage_V, &seg1MaxCellVoltage_V, &seg1MinCellVoltage_V},
			{&seg2Voltage_V, &seg2AveCellVoltage_V, &seg2MaxCellVoltage_V, &seg2MinCellVoltage_V},
			{&seg3Voltage_V, &seg3AveCellVoltage_V, &seg3MaxCellVoltage_V, &seg3MinCellVoltage_V},
			{&seg4Voltage_V, &seg4AveCellVoltage_V, &seg4MaxCellVoltage_V, &seg4MinCellVoltage_V},
			{&seg5Voltage_V, &seg5AveCellVoltage_V, &seg5MaxCellVoltage_V, &seg5MinCellVoltage_V},
			{&seg6Voltage_V, &seg6AveCellVoltage_V, &seg6MaxCellVoltage_V, &seg6MinCellVoltage_V},
			{&seg7Voltage_V, &seg7AveCellVoltage_V, &seg7MaxCellVoltage_V, &seg7MinCellVoltage_V}
		};

		static FLOAT_CAN_STRUCT *cellTempStatsParams[NUM_BMBS_IN_ACCUMULATOR][3] =
		{
			{&seg1AveCellTemp_C, &seg1MaxCellTemp_C, &seg1MinCellTemp_C},
			{&seg2AveCellTemp_C, &seg2MaxCellTemp_C, &seg2MinCellTemp_C},
			{&seg3AveCellTemp_C, &seg3MaxCellTemp_C, &seg3MinCellTemp_C},
			{&seg4AveCellTemp_C, &seg4MaxCellTemp_C, &seg4MinCellTemp_C},
			{&seg5AveCellTemp_C, &seg5MaxCellTemp_C, &seg5MinCellTemp_C},
			{&seg6AveCellTemp_C, &seg6MaxCellTemp_C, &seg6MinCellTemp_C},
			{&seg7AveCellTemp_C, &seg7MaxCellTemp_C, &seg7MinCellTemp_C}
		};

		static FLOAT_CAN_STRUCT *boardTempStatsParams[NUM_BMBS_IN_ACCUMULATOR][3] =
		{
			{&seg1BMBAveBoardTemp_C, &seg1BMBMaxBoardTemp_C, &seg1BMBMinBoardTemp_C},
			{&seg2BMBAveBoardTemp_C, &seg2BMBMaxBoardTemp_C, &seg2BMBMinBoardTemp_C},
			{&seg3BMBAveBoardTemp_C, &seg3BMBMaxBoardTemp_C, &seg3BMBMinBoardTemp_C},
			{&seg4BMBAveBoardTemp_C, &seg4BMBMaxBoardTemp_C, &seg4BMBMinBoardTemp_C},
			{&seg5BMBAveBoardTemp_C, &seg5BMBMaxBoardTemp_C, &seg5BMBMinBoardTemp_C},
			{&seg6BMBAveBoardTemp_C, &seg6BMBMaxBoardTemp_C, &seg6BMBMinBoardTemp_C},
			{&seg7BMBAveBoardTemp_C, &seg7BMBMaxBoardTemp_C, &seg7BMBMinBoardTemp_C}
		};

		static U8_CAN_STRUCT *balswenParams[NUM_BMBS_IN_ACCUMULATOR][NUM_BRICKS_PER_BMB] =
		{
			{&seg1Cell1BalanceEnable_state, &seg1Cell2BalanceEnable_state, &seg1Cell3BalanceEnable_state, &seg1Cell4BalanceEnable_state, &seg1Cell5BalanceEnable_state, &seg1Cell6BalanceEnable_state, &seg1Cell7BalanceEnable_state, &seg1Cell8BalanceEnable_state, &seg1Cell9BalanceEnable_state, &seg1Cell10BalanceEnable_state, &seg1Cell11BalanceEnable_state, &seg1Cell12BalanceEnable_state},
			{&seg2Cell1BalanceEnable_state, &seg2Cell2BalanceEnable_state, &seg2Cell3BalanceEnable_state, &seg2Cell4BalanceEnable_state, &seg2Cell5BalanceEnable_state, &seg2Cell6BalanceEnable_state, &seg2Cell7BalanceEnable_state, &seg2Cell8BalanceEnable_state, &seg2Cell9BalanceEnable_state, &seg2Cell10BalanceEnable_state, &seg2Cell11BalanceEnable_state, &seg2Cell12BalanceEnable_state},
			{&seg3Cell1BalanceEnable_state, &seg3Cell2BalanceEnable_state, &seg3Cell3BalanceEnable_state, &seg3Cell4BalanceEnable_state, &seg3Cell5BalanceEnable_state, &seg3Cell6BalanceEnable_state, &seg3Cell7BalanceEnable_state, &seg3Cell8BalanceEnable_state, &seg3Cell9BalanceEnable_state, &seg3Cell10BalanceEnable_state, &seg3Cell11BalanceEnable_state, &seg3Cell12BalanceEnable_state},
			{&seg4Cell1BalanceEnable_state, &seg4Cell2BalanceEnable_state, &seg4Cell3BalanceEnable_state, &seg4Cell4BalanceEnable_state, &seg4Cell5BalanceEnable_state, &seg4Cell6BalanceEnable_state, &seg4Cell7BalanceEnable_state, &seg4Cell8BalanceEnable_state, &seg4Cell9BalanceEnable_state, &seg4Cell10BalanceEnable_state, &seg4Cell11BalanceEnable_state, &seg4Cell12BalanceEnable_state},
			{&seg5Cell1BalanceEnable_state, &seg5Cell2BalanceEnable_state, &seg5Cell3BalanceEnable_state, &seg5Cell4BalanceEnable_state, &seg5Cell5BalanceEnable_state, &seg5Cell6BalanceEnable_state, &seg5Cell7BalanceEnable_state, &seg5Cell8BalanceEnable_state, &seg5Cell9BalanceEnable_state, &seg5Cell10BalanceEnable_state, &seg5Cell11BalanceEnable_state, &seg5Cell12BalanceEnable_state},
			{&seg6Cell1BalanceEnable_state, &seg6Cell2BalanceEnable_state, &seg6Cell3BalanceEnable_state, &seg6Cell4BalanceEnable_state, &seg6Cell5BalanceEnable_state, &seg6Cell6BalanceEnable_state, &seg6Cell7BalanceEnable_state, &seg6Cell8BalanceEnable_state, &seg6Cell9BalanceEnable_state, &seg6Cell10BalanceEnable_state, &seg6Cell11BalanceEnable_state, &seg6Cell12BalanceEnable_state},
			{&seg7Cell1BalanceEnable_state, &seg7Cell2BalanceEnable_state, &seg7Cell3BalanceEnable_state, &seg7Cell4BalanceEnable_state, &seg7Cell5BalanceEnable_state, &seg7Cell6BalanceEnable_state, &seg7Cell7BalanceEnable_state, &seg7Cell8BalanceEnable_state, &seg7Cell9BalanceEnable_state, &seg7Cell10BalanceEnable_state, &seg7Cell11BalanceEnable_state, &seg7Cell12BalanceEnable_state}
		};

		// Log gcan variables across the alloted time period in data chunks
		static uint32_t lastGcanUpdate = 0;
		if((HAL_GetTick() - lastGcanUpdate) >= GOPHER_CAN_LOGGING_PERIOD_MS)
		{
			lastGcanUpdate = HAL_GetTick();

			// Log the accumulator data according to the current GCAN logging state
			static uint8_t gcanUpdateState = 0;
			switch (gcanUpdateState)
			{
				case GCAN_SEGMENT_1:
				case GCAN_SEGMENT_2:
				case GCAN_SEGMENT_3:
				case GCAN_SEGMENT_4:
				case GCAN_SEGMENT_5:
				case GCAN_SEGMENT_6:
				case GCAN_SEGMENT_7:
					
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][0], gBms.bmb[gcanUpdateState].segmentV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][1], gBms.bmb[gcanUpdateState].avgBrickV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][2], gBms.bmb[gcanUpdateState].maxBrickV);
					update_and_queue_param_float(cellVoltageStatsParams[gcanUpdateState][3], gBms.bmb[gcanUpdateState].minBrickV);
					

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_float(cellVoltageParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].brickV[i]);
					}

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_float(cellTempParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].brickTemp[i]);
					}

					for (int32_t i = 0; i < NUM_BOARD_TEMP_PER_BMB; i++)
					{
						update_and_queue_param_float(boardTempParams[gcanUpdateState][i], gBms.bmb[gcanUpdateState].boardTemp[i]);
					}
					break;
				
				case GCAN_CELL_TEMP_STATS:
					for (int32_t i = 0; i < NUM_BMBS_IN_ACCUMULATOR; i++)
					{
						update_and_queue_param_float(cellTempStatsParams[i][0], gBms.bmb[i].avgBrickTemp);
						update_and_queue_param_float(cellTempStatsParams[i][1], gBms.bmb[i].maxBrickTemp);
						update_and_queue_param_float(cellTempStatsParams[i][2], gBms.bmb[i].minBrickTemp);
					}

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_u8(balswenParams[0][i], gBms.bmb[0].balSwEnabled[i]);
					}

					update_and_queue_param_u8(&amsFault_state, gBms.amsFaultStatus);
					update_and_queue_param_u8(&bspdFault_state, gBms.bspdFaultStatus);

					break;

				case GCAN_BOARD_TEMP_STATS:
					for (int32_t i = 0; i < NUM_BMBS_IN_ACCUMULATOR; i++)
					{
						update_and_queue_param_float(boardTempStatsParams[i][0], gBms.bmb[i].avgBoardTemp);
						update_and_queue_param_float(boardTempStatsParams[i][1], gBms.bmb[i].maxBoardTemp);
						update_and_queue_param_float(boardTempStatsParams[i][2], gBms.bmb[i].minBoardTemp);
					}

					for (int32_t i = 0; i < NUM_BRICKS_PER_BMB; i++)
					{
						update_and_queue_param_u8(balswenParams[1][i], gBms.bmb[1].balSwEnabled[i]);
					}

					update_and_queue_param_u8(&imdFault_state, gBms.imdFaultStatus);
					update_and_queue_param_u8(&imdFaultInfo_state, gBms.imdState);
					
					break;

				case GCAN_BALSWEN:
					for (int32_t i = 2; i < NUM_BMBS_IN_ACCUMULATOR; i++)
					{
						for (int32_t j = 0; j < NUM_BRICKS_PER_BMB; j++)
						{
							update_and_queue_param_u8(balswenParams[i][j], gBms.bmb[i].balSwEnabled[j]);
						}
					}

					// Multiply SOE values by 100 to convert to percent before sending over gcan
					// SOE values sent over gcan maintain 2 values after decimal place of percentage. Ex: 98.34% 
					update_and_queue_param_float(&soeByOCV_percent, gBms.soc.soeByOcv * 100.0f);
					update_and_queue_param_float(&soeByCoulombCounting_percent, gBms.soc.soeByCoulombCounting * 100.0f);

					//TODO Potentially add sensor status bytes
					break;

				default:
					gcanUpdateState = 0;
					break;
			}

			// Cycle Gcan state and wrap to 0 if needed
			gcanUpdateState++;
			gcanUpdateState %= NUM_GCAN_STATES;

			service_can_tx(&hcan2);
		}
	}
}

void checkForNewChargerInfo()
{
	static uint32_t lastChargerRX = 0;
	if (newChargerMessage)
	{
		lastChargerRX = HAL_GetTick();
		gBms.chargerConnected = true;
		updateChargerData(&gBms.chargerData);
		newChargerMessage = false;
	}
	if (gBms.chargerConnected && ((HAL_GetTick() - lastChargerRX) > CHARGER_RX_TIMEOUT_MS))
	{
		gBms.chargerConnected = false;
	}
}

/*!
  @brief   Perform accumulator charge sequence
*/
void chargeAccumulator()
{
	// Periodically send an updated charger request CAN message to the charger
	// Second condition protects from case in which a new charger message is recieved between this if statement and the last
	static uint32_t lastChargerUpdate = 0;
	if (((HAL_GetTick() - lastChargerUpdate) >= CHARGER_UPDATE_PERIOD_MS) && (!newChargerMessage))
	{
		lastChargerUpdate = HAL_GetTick();

		// Voltage and current request defauult to 0
		float voltageRequest = 0.0f;
		float currentRequest = 0.0f;
		bool chargeOkay = false;

		if(!gBms.chargingDisabled)
		{
			// Cell Imbalance hysteresis
			// cellImbalancePresent set when pack cell imbalance exceeds high threshold
			// cellImbalancePresent reset when pack cell imbalance falls under low threshold
			static bool cellImbalancePresent = false;
			float cellImbalance = gBms.maxBrickV - gBms.minBrickV;
			if(cellImbalance > MAX_CELL_IMBALANCE_THRES_HIGH)
			{
				cellImbalancePresent = true;
			}
			else if(cellImbalance < MAX_CELL_IMBALANCE_THRES_LOW)
			{
				cellImbalancePresent = false;
			}
			else
			{
				// Maintain state of cellImbalancePresent
			}

			// Cell Over Voltage hysteresis
			// cellOverVoltagePresent set when pack max cell voltage exceeds high threshold
			// cellOverVoltagePresent reset when pack max cell voltage falls under low threshold
			static bool cellOverVoltagePresent = false;
			if(gBms.maxBrickV > MAX_CELL_VOLTAGE_THRES_HIGH)
			{
				cellOverVoltagePresent = true;
			}
			else if(gBms.maxBrickV < MAX_CELL_VOLTAGE_THRES_LOW)
			{
				cellOverVoltagePresent = false;
			}
			else
			{
				// Maintain state of cellOverVoltagePresent
			}

			// Charging is only allowed if the bms disable, cellImbalancePresent, and cellOverVoltagePresent are all not true 
			chargeOkay = !(gBms.chargingDisabled || cellImbalancePresent || cellOverVoltagePresent);
			if(chargeOkay)
			{
				// Always request max charging voltage
				voltageRequest = MAX_CHARGE_VOLTAGE_V;

				// If the min cell voltage is below the low charge voltage threshold, the charge current is reduced
				if(gBms.minBrickV < LOW_CHARGE_VOLTAGE_THRES_V)
				{
					// This is a safe chage current for low voltage cells, typically 1/10 C
					currentRequest = LOW_CHARGE_CURRENT_A;
				}
				else
				{
					// This is the normal charge current set in charger.h
					currentRequest = HIGH_CHARGE_CURRENT_A;
				}

				// Calculate the max current possible given the charger power and pack voltage
				float powerLimitAmps = (CHARGER_INPUT_POWER_W * MIN_CHARGER_EFFICIENCY) / (gBms.accumulatorVoltage + CHARGER_VOLTAGE_MISMATCH_THRESHOLD); // TODO factor in eff and safety zone

				// Limit the charge current to the charger max power output if necessary 
				if(currentRequest > powerLimitAmps)
				{
					currentRequest = powerLimitAmps;
				}
			}
		}

		// Send the calculated voltage and current requests to the charger over CAN
		sendChargerMessage(voltageRequest, currentRequest, chargeOkay);
	}
}
