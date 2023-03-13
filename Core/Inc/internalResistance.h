#ifndef INC_INTERNAL_RESISTANCE_H_
#define INC_INTERNAL_RESISTANCE_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "bms.h"
#include "currentSense.h"
#include "bmb.h"


/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define SHIFTING_WINDOW_SIZE_MS 5000
#define AVERAGE_BUFFER_SIZE 10

#define DISCRETE_CURRENT_BUFFER_SIZE    SHIFTING_WINDOW_SIZE_MS / (AVERAGE_BUFFER_SIZE * CURRENT_SENSOR_UPDATE_PERIOD_MS)
#define DISCRETE_VOLTAGE_BUFFER_SIZE    SHIFTING_WINDOW_SIZE_MS / (AVERAGE_BUFFER_SIZE * BMB_DATA_REFRESH_DELAY_MS)

#define IR_CALC_MIN_CURRENT_DELTA   1

#define IR_BAD_DATA -1000.0f


/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Put bms current data into discrete data buffer
  @param   bms - BMS data struct
*/
void putCurrentBuffer(Bms_S* bms);

/*!
  @brief   Put bms voltage data into discrete data buffer
  @param   bms - BMS data struct
*/
void putVoltageBuffer(Bms_S* bms);


#endif /* INC_INTERNAL_RESISTANCE_H_ */
