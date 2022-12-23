#include "main.h"
#include "cmsis_os.h"
#include "bmb.h"
#include "spiUtils.h"
#include "bmbUtils.h"

extern osSemaphoreId binSemHandle;

uint8_t recvBuffer[SPI_BUFF_SIZE];
uint8_t sendBuffer[SPI_BUFF_SIZE];

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

	// Enable GPIOs as outputs
	writeAll(GPIO, 0xF000, numBmbs);

	// Set brickOV voltage alert threshold
	// Set brickUV voltage alert threshold

}

// TODO - Isaiah fix
void setGpio(uint32_t numBmbs, uint16_t gpioSetting)
{
	// Assuming all GPIOs configured as OUTPUT
	uint16_t data = 0xF000;
	data |= (gpioSetting & 0x000F);	// Extract last 4 bits from gpioSetting
	writeAll(GPIO, data, numBmbs);
}

// TODO - Isaiah Fix
void readBoardTemps(Bmb_S* bmb, uint32_t numBmbs)
{
	// Internal board temps are on MUX7 and MUX8
	for (int muxGpio = MUX7; muxGpio <= MUX8; muxGpio++)
	{
		setGpio(numBmbs, muxGpio);		// Set GPIO for desired MUX
		// Start acquisition
		writeAll(SCANCTRL, 0x0001, numBmbs);
		// TODO implement error checking in the case of bad data or broken comms
		// Read AUX registers
		for (int auxChannel = AIN1; auxChannel <= AIN2; auxChannel++)
		{
			// Temperature index for Bmb_S struct board temp
			int tempIdx = muxGpio - MUX7 + ((auxChannel == AIN2) ? 2 : 0);
			// Read temperature from BMBs
			if (readAll(auxChannel, recvBuffer, numBmbs))
			{
				// Parse received data
				for (uint8_t i = 0; i < numBmbs; i++)
				{
					// Read AUX voltage in [15:4]
					uint32_t auxRaw = ((recvBuffer[4 + 2*i] << 8) | recvBuffer[3 + 2*i]) >> 4;
					float auxV = auxRaw * CONVERT_12BIT_TO_3V3;
					// Determine boardTempVoltage index for current reading
					bmb[i].boardTempVoltage[tempIdx] = auxV;
				}
			}
			else
			{
				// Failed to acquire data. Set status to MIA
				for (int i = 0; i < numBmbs; i++)
				{
					bmb[i].boardTempStatus[tempIdx] = MIA;
				}
			}
		}
	}

	for (uint8_t i = 0; i < numBmbs; i++)
	{
		for (int j = 0; j < NUM_BOARD_TEMP_PER_BMB; j++)
		{
			float ntcV = bmb[i].boardTempVoltage[j];

			bmb[i].boardTemp[j] = lookup(ntcV, &ntcTable);
		}
	}


}

// TODO - Isaiah fix
void updateBmbData(Bmb_S* bmb, uint32_t numBmbs)
{
	// Start acquisition
	writeAll(SCANCTRL, 0x0001, numBmbs);
	// Update cell data
	// TODO implement error checking in the case of bad data or broken comms
	for (uint8_t i = 0; i < 12; i++)
	{
		uint8_t cellReg = i + CELLn;
		if (!readAll(cellReg, recvBuffer, numBmbs))
		{
			printf("Error during readAll!\r\n");
			break;
		}
		for (uint8_t j = 0; j < numBmbs; j++)
		{
			// Read brick voltage in [15:2]
			uint32_t brickVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
			float brickV = brickVRaw * CONVERT_14BIT_TO_5V;
			bmb[j].brickV[i] = brickV;
		}
	}
	// Read VBLOCK register
	if (!readAll(VBLOCK, recvBuffer, numBmbs))
	{
		for (uint8_t j = 0; j < numBmbs; j++)
		{
			// Read block voltage in [15:2]
			uint32_t blockVRaw = ((recvBuffer[4 + 2*j] << 8) | recvBuffer[3 + 2*j]) >> 2;
			float blockV = blockVRaw * CONVERT_14BIT_TO_60V;
			bmb[j].blockV = blockV;
		}
	}
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
