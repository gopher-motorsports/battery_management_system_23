#ifndef INC_EPAPER_H_
#define INC_EPAPER_H_


/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "main.h"
#include "bms.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// Display resolution
#define EPD_WIDTH                       128
#define EPD_HEIGHT                      296

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DECLARATIONS =================== */
/* ==================================================================== */

/*!
  @brief	Creates the default BMS display template in memory
*/
void epdInit();

/*!
  @brief	Update the data in the BMS display image
  @param epapData The epapData struct containing current BMS data 
*/
void epdPopulateData(Epaper_Data_S* epapData);

/*!
  @brief	Clear epaper display
*/
void epdClear();

/*!
  @brief	Performs a full update of the epaper with the current BMS display image stored in memory
*/
void epdFullRefresh();

/*!
  @brief	Performs a partial update of the epaper with the current BMS display image stored in memory
*/
void epdPartialRefresh();

/*!
  @brief	Enable epaper sleep mode
*/
void epdSleep();

#endif /* INC_EPAPER_H_ */
