#ifndef INC_SHARED_H_
#define INC_SHARED_H_

// Responses for a specific alert
typedef enum
{
    INFO_ONLY = 0,
    DISABLE_BALANCING,
    EMERGENCY_BLEED,
    STOP_CHARGING,
    LIMP_MODE,
    AMS_FAULT,
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
