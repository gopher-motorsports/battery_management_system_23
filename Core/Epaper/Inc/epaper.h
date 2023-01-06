#ifndef INC_EPAPER_H_
#define INC_EPAPER_H_

/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */
#include "main.h"

/* ==================================================================== */
/* ============================= DEFINES ============================== */
/* ==================================================================== */

// Display resolution
#define EPD_WIDTH                       128
#define EPD_HEIGHT                      296

// Lookup table size
#define LUT_SIZE                        159

// Update size, given by resolution in bytes
#define UPDATE_BYTES                    (EPD_WIDTH * EPD_HEIGHT) / 8

// Command Registers
#define CMD_DRIVER_OUTPUT_CONTROL           0x01
#define CMD_GATE_DRIVING_VOLTAGE_CONTROL    0x03
#define CMD_SOURCE_DRIVING_VOLTAGE_CONTROL  0x04
#define CMD_DEEP_SLEEP_MODE                 0x10
#define CMD_DATA_ENTRY_MODE_SETTING         0x11
#define CMD_SW_RESET                        0x12
#define CMD_MASTER_ACTIVATION               0x20
#define CMD_DISPLAY_UPDATE_CONTROL_1        0x21
#define CMD_DISPLAY_UPDATE_CONTROL_2        0x22
#define CMD_WRITE_RAM_BLACK_WHITE           0x24
#define CMD_WRITE_RAM_RED                   0x26
#define CMD_WRITE_VCOM_REGISTER             0x2C
#define CMD_WRITE_LUT_REGISTER              0x32
#define CMD_WRITE_MYSTERY_REGISTER_1        0x37
#define CMD_BORDER_WAVEFORM_CONTROL         0x3C
#define CMD_WRITE_MYSTERY_REGISTER_2        0x3f
#define CMD_SET_RAM_X_WINDOW                0x44
#define CMD_SET_RAM_Y_WINDOW                0x45
#define CMD_SET_RAM_X_COUNTER               0x4E
#define CMD_SET_RAM_Y_COUNTER               0x4F
/* ==================================================================== */
/* ========================= ENUMERATED TYPES ========================= */
/* ==================================================================== */

typedef enum
{
    UNINITIALIZED = 0,
    FULL_REFRESH,
    PARTIAL_REFRESH
} Epaper_Configuration_E;

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

void epdClear();
void epdDisplay(uint8_t *Image);
void epdDisplayPartial(uint8_t *Image);
void epdSleep();

#endif /* INC_EPAPER_H_ */