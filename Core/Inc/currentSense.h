#ifndef INC_CURRENT_SENSE_H_
#define INC_CURRENT_SENSE_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "bms.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define CURRENT_HIGH_RAIL_THRESHOLD             590//420
#define CURRENT_LOW_RAIL_THRESHOLD              87//36
#define CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD    75//30

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Update the bms tractive system current data and sensor status from gophercan
  @param   bms - BMS data struct
*/
void getTractiveSystemCurrent(Bms_S* bms);


#endif /* INC_CURRENT_SENSE_H_ */
