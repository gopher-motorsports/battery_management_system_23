#ifndef INC_CURRENT_SENSE_H_
#define INC_CURRENT_SENSE_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "bms.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define CURRENT_HIGH_RAIL_THRESHOLD             420
#define CURRENT_LOW_RAIL_THRESHOLD              36
#define CURRENT_LOW_TO_HIGH_SWITCH_THRESHOLD    30

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief   Update the bms tractive system current data and sensor status from gophercan
  @param   bms - BMS data struct
*/
void getTractiveSystemCurrent(Bms_S* bms);


#endif /* INC_CURRENT_SENSE_H_ */
