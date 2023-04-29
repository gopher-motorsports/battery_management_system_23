#ifndef INC_SHARED_H_
#define INC_SHARED_H_

#include <stdint.h>

/* ==================================================================== */
/* ========================= ENUMERATED TYPES ========================= */
/* ==================================================================== */

// Responses for a specific alert
typedef enum
{
    INFO_ONLY = 0,		// Only used for info no actual response
    DISABLE_BALANCING,	// Disables cell balancing
    EMERGENCY_BLEED,	// Emergencly bleed all the cells down
    DISABLE_CHARGING,	// Disable charging 
    LIMP_MODE,			// Limit max current out of pack
    AMS_FAULT,			// Set AMS fault to open shutdown circuit
    NUM_ALERT_RESPONSES
} AlertResponse_E;

typedef enum
{
    CHARGER_HARDWARE_FAILURE_ERROR,
    CHARGER_OVERTEMP_ERROR,
    CHARGER_INPUT_VOLTAGE_ERROR,
    CHRAGER_BATTERY_NOT_DETECTED_ERROR,
    CHARGER_COMMUNICATION_ERROR,
    NUM_CHARGER_FAULTS
} Charger_Error_E;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct
{
  	float chargerVoltage;
	float chargerCurrent;
	bool chargerStatus[NUM_CHARGER_FAULTS];

} Charger_Data_S;

typedef struct
{
	float maxBrickV;
	float minBrickV;
	float avgBrickV;

	float maxBrickTemp;
	float minBrickTemp;
	float avgBrickTemp;

	float maxBoardTemp;
	float minBoardTemp;
	float avgBoardTemp;

	float current;

	char* stateMessage;

	char* alertMessage;
	uint32_t alertIndex;
	uint32_t numActiveAlerts;
} Epaper_Data_S;

#endif /* INC_SHARED_H_ */
