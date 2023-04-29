#ifndef INC_SHARED_H_
#define INC_SHARED_H_

#include <stdint.h>

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

	char* stateMessage;			// String of bms state to be displayed on epaper

	char* alertMessage;			// String of alert message to be displayed on epaper
	uint32_t currAlertIndex;	// The index of currenly active alerts
	uint32_t numActiveAlerts;	// The number of active alerts, if 0, no alerts will be displayed
} Epaper_Data_S;

#endif /* INC_SHARED_H_ */
