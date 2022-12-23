#include "main.h"
#include "cmsis_os.h"
#include "bmb.h"
#include "spiUtils.h"
#include "bmbUtils.h"

extern osSemaphoreId binSemHandle;

uint8_t recvBuffer[SPI_BUFF_SIZE];
uint8_t sendBuffer[SPI_BUFF_SIZE];

static Mux_State_E muxState = 0x00;
static bool gpio3State = 0;
static uint32_t lastUpdate = 0;

/*!
  @brief   Initialize ASCI and BMB daisy chain. Enumerate BMBs
  @param   numBmbs - Updated with number of enumerated BMBs from HELLOALL command
  @return  True if successful initialization, false otherwise
*/
bool initASCI(uint32_t *numBmbs)
{
	disableASCI();
	HAL_Delay(10);
	//Test
	enableASCI();
	ssOff();
	bool successfulConfig = true;
	// dummy transaction since this chip sucks
	readRegister(R_CONFIG_3);

	// Set Keep_Alive to 0x05 = 160us
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_3, 0x05);

	// Enable RX_Error, RX_Overflow and RX_Busy interrupts
	successfulConfig &= writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0xA8);

	clearRxBuffer();

	// Enable TX_Preambles mode
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_2, 0x30);

	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during initialization!\r\n");
		return false;
	}

	// Verify interrupt was caused by RX_Busy
	if (readRxBusyFlag())
	{
		clearRxBusyFlag();
	}
	else
	{
		successfulConfig = false;
	}

	// Verify RX_Busy_Status and RX_Empty_Status true
	successfulConfig &= (readRegister(R_RX_STATUS) == 0x21);

	// Enable RX_Stop INT
	successfulConfig &= writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0x8A);

	// Enable TX_Queue mode
	successfulConfig &= writeAndVerifyRegister(R_CONFIG_2, 0x10);

	// Verify RX_Empty
	successfulConfig &= !!(readRegister(R_RX_STATUS) & 0x01);

	clearTxBuffer();
	clearRxBuffer();

	// Send Hello_All command
	sendBuffer[0] = CMD_WR_LD_Q_L0;
	sendBuffer[1] = 0x03;			// Data length
	sendBuffer[2] = CMD_HELLO_ALL;	// HELLOALL command byte
	sendBuffer[3] = 0x00;			// Register address
	sendBuffer[4] = 0x00;			// Initialization address for HELLOALL

	if (!loadAndVerifyTxQueue((uint8_t *)&sendBuffer, 5))
	{
		printf("Failed to load all!\r\n");
		return false;
	}

	// Enable RX_Error, RX_Overflow and RX_Stop interrupts
	successfulConfig &= writeAndVerifyRegister(R_RX_INTERRUPT_ENABLE, 0x8A);

	sendSPI(CMD_WR_NXT_LD_Q_L0);


	if (!(xSemaphoreTake(binSemHandle, 10) == pdTRUE))
	{
		printf("Interrupt failed to occur during initialization!\r\n");
		return false;
	}

	if (readRxStopFlag())
	{
		clearRxStopFlag();
	}
	else
	{
		successfulConfig = false;
	}

	// Verify stop bit
	successfulConfig &= !!(readRegister(R_RX_STATUS) & 0x02);


	successfulConfig &= readNextSpiMessage((uint8_t *)&recvBuffer, 3);

	if (rxErrorsExist() || !successfulConfig)
	{
		printf("Detected errors during initialization...\r\n");
		return false;
	}

	(*numBmbs) = recvBuffer[3];

	return true;
}

/*!
  @brief   Initialize the BMBs by configuring registers
  @param   numBmbs - The expected number of BMBs in the daisy chain
  @return  True if successful initialization, false otherwise
*/
//TODO make this return bool
void initBmbs(uint32_t numBmbs)
{
	// Enable alive counter byte
	// numBmbs set to 0 since alive counter not yet enabled
	writeAll(DEVCFG1, 0x1042, 0);

	// Enable measurement channels
	writeAll(MEASUREEN, 0xFFFF, numBmbs);

	// Reset GPIO to 0 state
	setGpio(numBmbs, 0, 0, 0, 0);

	// Set brickOV voltage alert threshold
	// Set brickUV voltage alert threshold

}

void cyclicUpdateBMBData(Bmb_S* bmb, uint32_t numBmbs)
{
	if((HAL_GetTick() - lastUpdate) >= DATA_REFRESH_DELAY_MILLIS)
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
				printf("Error during cellReg readAll!\r\n");

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
			printf("Error during VBLOCK readAll!\r\n");

			// Failed to acquire data. Set status to MIA
			for (int j = 0; j < numBmbs; j++)
			{
				bmb[j].blockVStatus = MIA;
			}
		}

		// TODO Add check - Compare VBLOCK with sum of brick voltages

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
						bmb[j].boardTemp[muxState - MUX7 + ((auxChannel == AIN2) ? 2 : 0)] = lookup(auxV, &ntcTable);
					}
					else // Zener/Brick Temp Channel
					{
						bmb[j].brickTemp[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = lookup(auxV, &zenerTable);
					}
				}
			}
			else
			{
				// Failed to acquire data. Set status to MIA
				for (int j = 0; j < numBmbs; j++)
				{
					bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? 8 : 0)] = MIA;
				}
			}
		}

		// Cycle to next MUX configuration
		setMux(numBmbs, (muxState + 1) % NUM_MUX_CHANNELS	);

		// Update lastUpdate
		lastUpdate = HAL_GetTick();
	}
}


void updateBMBVoltageData(Bmb_S* bmb, uint32_t numBmbs)
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
			printf("Error during cellReg readAll!\r\n");

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
		printf("Error during VBLOCK readAll!\r\n");

		// Failed to acquire data. Set status to MIA
		for (int j = 0; j < numBmbs; j++)
		{
			bmb[j].blockVStatus = MIA;
		}
	}

	// TODO Add check - Compare VBLOCK with sum of brick voltages
}

void updateBMBTempData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Cycle through MUX channels
	setMux(numBmbs, MUX1);
	for(int i = MUX1; i < NUM_MUX_CHANNELS; i++)
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
					if(muxState == (MUX7 || MUX8)) // NTC/ON-Board Temp Channel
					{
						bmb[j].boardTemp[muxState - MUX7 + ((auxChannel == AIN2) ? 2 : 0)] = lookup(auxV, &ntcTable);
					}
					else // Zener/Brick Temp Channel
					{
						bmb[j].brickTemp[muxState + ((auxChannel == AIN2) ? NUM_MUX_CHANNELS : 0)] = lookup(auxV, &zenerTable);
					}
				}
			}
			else
			{
				// Failed to acquire data. Set status to MIA
				for (int j = 0; j < numBmbs; j++)
				{
					bmb[j].tempStatus[muxState + ((auxChannel == AIN2) ? 8 : 0)] = MIA;
				}
			}
		}
	}
}

void setMux(uint32_t numBmbs, uint8_t muxSetting)
{
	bool gpio[3];
	for(int i = 0; i < 3; i++)
	{
		gpio[i] = (muxSetting >> i) & 1;
	}
	setGpio(numBmbs, gpio[0], gpio[1], gpio[2], gpio3State); // Currently sets GPIO 4 to 0 when updating MUX
}

void setGpio(uint32_t numBmbs, bool gpio0, bool gpio1, bool gpio2, bool gpio3)
{
	// First 4 bits set GPIO to output mode
	// Last 4 bits set GPIO logic state for channels 3, 2, 1, 0 respectively
	uint16_t data = 0xF000 | (gpio0) | (gpio1 << 1) | (gpio2 << 2) | (gpio3 << 3);
	writeAll(GPIO, data, numBmbs);

	// Update Mux state depending on only GPIO settings 0, 1, 2
	muxState = data & 0x0007;

	// Update gpio4 state
	gpio3State = gpio3;
}

/*!
  @brief   Update BMB data statistics. Min/Max/Avg
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
