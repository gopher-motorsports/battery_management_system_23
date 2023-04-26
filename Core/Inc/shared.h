#ifndef INC_SHARED_H_
#define INC_SHARED_H_

// Responses for a specific alert
typedef enum
{
    INFO_ONLY = 0,		// Only used for info no actual response
    DISABLE_BALANCING,	// Disables cell balancing
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
} Epaper_Data_S;

#endif /* INC_SHARED_H_ */
