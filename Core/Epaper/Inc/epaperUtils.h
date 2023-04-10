#ifndef __EPAPER_UTILS_H
#define __EPAPER_UTILS_H

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "fonts.h"
#include <stdint.h>

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

#define ROTATE_0            0
#define ROTATE_90           90
#define ROTATE_180          180
#define ROTATE_270          270

#define DATA_START_X        43
#define DATA_START_Y        63
#define DATA_SEPERATION_X   63
#define DATA_SEPERATION_Y   21

#define WHITE               0xFF
#define BLACK               0x00

#define IMAGE_BACKGROUND    WHITE
#define FONT_FOREGROUND     BLACK
#define FONT_BACKGROUND     WHITE

/* ==================================================================== */
/* ========================= ENUMERATED TYPES========================== */
/* ==================================================================== */

typedef enum {
    MIRROR_NONE  = 0x00,
    MIRROR_HORIZONTAL = 0x01,
    MIRROR_VERTICAL = 0x02,
    MIRROR_ORIGIN = 0x03,
} MIRROR_IMAGE;
#define MIRROR_IMAGE_DFT MIRROR_NONE

typedef enum {
    DOT_PIXEL_1X1  = 1,		// 1 x 1
    DOT_PIXEL_2X2  , 		// 2 X 2
    DOT_PIXEL_3X3  ,		// 3 X 3
    DOT_PIXEL_4X4  ,		// 4 X 4
    DOT_PIXEL_5X5  , 		// 5 X 5
    DOT_PIXEL_6X6  , 		// 6 X 6
    DOT_PIXEL_7X7  , 		// 7 X 7
    DOT_PIXEL_8X8  , 		// 8 X 8
} DOT_PIXEL;
#define DOT_PIXEL_DFT  DOT_PIXEL_1X1  //Default dot pilex

typedef enum {
    DOT_FILL_AROUND  = 1,		// dot pixel 1 x 1
    DOT_FILL_RIGHTUP  , 		// dot pixel 2 X 2
} DOT_STYLE;
#define DOT_STYLE_DFT  DOT_FILL_AROUND  //Default dot pilex

typedef enum {
    LINE_STYLE_SOLID = 0,
    LINE_STYLE_DOTTED,
} LINE_STYLE;

typedef enum {
    DRAW_FILL_EMPTY = 0,
    DRAW_FILL_FULL,
} DRAW_FILL;

typedef enum {
    DATA_VOLTAGE = 0,
    DATA_PACK_TEMP,
    DATA_BOARD_TEMP
}   DATA_TABLE_COL;

typedef enum {
    DATA_AVG = 0,
    DATA_MAX,
    DATA_MIN
}   DATA_TABLE_ROW;

/* ==================================================================== */
/* ============================== STRUCTS============================== */
/* ==================================================================== */

typedef struct {
    uint16_t Year;  //0000
    uint8_t  Month; //1 - 12
    uint8_t  Day;   //1 - 30
    uint8_t  Hour;  //0 - 23
    uint8_t  Min;   //0 - 59
    uint8_t  Sec;   //0 - 59
} PAINT_TIME;

typedef struct {
    uint8_t *Image;
    uint16_t Width;
    uint16_t Height;
    uint16_t WidthMemory;
    uint16_t HeightMemory;
    uint16_t Color;
    uint16_t Rotate;
    uint16_t Mirror;
    uint16_t WidthByte;
    uint16_t HeightByte;
    uint16_t Scale;
} PAINT;

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief    Initialize BMS template image
  @param    emptyImage Empty image array to modify with BMS data 
*/
void Paint_InitBmsImage(uint8_t* emptyImage);

/*!
  @brief    Update BMS Image with current voltage, pack temp, and board temp data
  @param    data Data to display on table
  @param    col Column of data table to populate
  @param    row Row of data table to populate 
*/
void Paint_DrawTableData(float data, DATA_TABLE_COL col, DATA_TABLE_ROW row);

/*!
  @brief    Update BMS Image with current SOC
  @param    SOC BMS State of Charge as a percentage 
*/
void Paint_DrawSOC(uint32_t SOC);

/*!
  @brief   Update BMS Image with state data
*/
void Paint_DrawState();

/*!
  @brief   Update BMS Image with fault data
*/
void Paint_DrawFault();

#endif // __EPAPER_UTILS_H





