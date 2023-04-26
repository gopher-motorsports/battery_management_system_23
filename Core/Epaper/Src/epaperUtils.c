/* ==================================================================== */
/* ============================= INCLUDES ============================= */
/* ==================================================================== */

#include "epaperUtils.h"
#include "debug.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h> //memset()
#include <math.h>
#include "epaper.h"

#define MAX_PRINT_LENGTH   60

#define TABLE_START_X        43
#define TABLE_START_Y        63

#define TABLE_COL_STEP_1    74
#define TABLE_COL_STEP_2    63

#define TABLE_ROW_STEP_1    21
#define TABLE_ROW_STEP_2    21

PAINT Paint;

uint32_t waveImage[2] =
{
    0b00000001000000000011100000000001,
    0b00000001110000001111111000000111
};

uint32_t tableSpacingCol[NUM_DATA_TABLE_COL-1] = 
{
    TABLE_COL_STEP_1, TABLE_COL_STEP_2
};
uint32_t tableSpacingRow[NUM_DATA_TABLE_ROW-1] =
{
    TABLE_ROW_STEP_1, TABLE_ROW_STEP_2
};


/* ==================================================================== */
/* =================== LOCAL FUNCTION DEFINITIONS ===================== */
/* ==================================================================== */

/******************************************************************************
function: Create Image
parameter:
    image   :   Pointer to the image cache
    width   :   The width of the picture
    Height  :   The height of the picture
    Color   :   Whether the picture is inverted
******************************************************************************/
static void Paint_NewImage(uint8_t *image, uint16_t Width, uint16_t Height, uint16_t Rotate, uint16_t Color)
{
    Paint.Image = NULL;
    Paint.Image = image;

    Paint.WidthMemory = Width;
    Paint.HeightMemory = Height;
    Paint.Color = Color;    
	Paint.Scale = 2;
		
    Paint.WidthByte = (Width % 8 == 0) ? (Width / 8 ) : (Width / 8 + 1);
    Paint.HeightByte = Height;
   
    Paint.Rotate = Rotate;
    Paint.Mirror = MIRROR_NONE;
    
    if(Rotate == ROTATE_0 || Rotate == ROTATE_180) {
        Paint.Width = Width;
        Paint.Height = Height;
    } else {
        Paint.Width = Height;
        Paint.Height = Width;
    }
}

static void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color)
{
    if(Xpoint > Paint.Width || Ypoint > Paint.Height){
        Debug("Epaper pixel request out of display bounds!\n");
        return;
    }

    uint32_t targetAddress = (Xpoint / 8) + (Ypoint * Paint.WidthByte);
    uint8_t targetByte = Paint.Image[targetAddress];

    if(Color == BLACK)
    {
        Paint.Image[targetAddress] = targetByte & ~(0x80 >> (Xpoint % 8));
    } 
    else
    {
        Paint.Image[targetAddress] = targetByte | (0x80 >> (Xpoint % 8));
    }
}

static void Paint_Clear(uint16_t Color)
{
	for (uint16_t y = 0; y < Paint.HeightByte; y++)
    {
        for (uint16_t x = 0; x < Paint.WidthByte; x++)
        {
            uint32_t address = x + y * Paint.WidthByte;
            Paint.Image[address] = Color;
        }
    }
}

static void Paint_ClearWindows(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd, uint16_t color)
{
    for (uint16_t y = yStart; y < yEnd; y++)
    {
        for (uint16_t x = xStart; x < xEnd; x++) {//8 pixel =  1 byte
            Paint_SetPixel(x, y, color);
        }
    }
}

static void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color, DOT_PIXEL Dot_Pixel)
{

    for (int16_t x = -1 * (Dot_Pixel - 1) / 2; x < (Dot_Pixel / 2) + 1; x++)
    {
        for (int16_t y = -1 * (Dot_Pixel - 1); y < (Dot_Pixel / 2) + 1; y++)
        {
            uint16_t targetX = Xpoint + x;
            uint16_t targetY = Ypoint + y;
            if((targetX < 0) || (targetX > Paint.Width))    { break; }
            if((targetY < 0) || (targetY > Paint.Height))   { break; }
            Paint_SetPixel(targetX, targetY, Color);
        }
    }
}

static void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style)
{
    uint16_t Xpoint = Xstart;
    uint16_t Ypoint = Ystart;
    int16_t dx = abs(Xend - Xstart);
    int16_t dy = abs(Yend - Ystart);

    // Increment direction, 1 is positive, -1 is counter;
    int16_t XAddway = Xstart < Xend ? 1 : -1;
    int16_t YAddway = Ystart < Yend ? 1 : -1;

    //Cumulative error
    int16_t Esp = dx + dy;
    char Dotted_Len = 0;

    for (;;) {
        Dotted_Len++;
        //Painted dotted line, 2 point is really virtual
        if (Line_Style == LINE_STYLE_DOTTED && Dotted_Len % 3 == 0) {
            //Debug("LINE_DOTTED\r\n");
            Paint_DrawPoint(Xpoint, Ypoint, IMAGE_BACKGROUND, Line_width);
            Dotted_Len = 0;
        } else {
            Paint_DrawPoint(Xpoint, Ypoint, Color, Line_width);
        }
        if (2 * Esp >= dy) {
            if (Xpoint == Xend)
                break;
            Esp += dy;
            Xpoint += XAddway;
        }
        if (2 * Esp <= dx) {
            if (Ypoint == Yend)
                break;
            Esp += dx;
            Ypoint += YAddway;
        }
    }
}

/******************************************************************************
function: Draw a rectangle
parameter:
    Xstart ：Rectangular  Starting Xpoint point coordinates
    Ystart ：Rectangular  Starting Xpoint point coordinates
    Xend   ：Rectangular  End point Xpoint coordinate
    Yend   ：Rectangular  End point Ypoint coordinate
    Color  ：The color of the Rectangular segment
    Line_width: Line width
    Draw_Fill : Whether to fill the inside of the rectangle
******************************************************************************/
static void Paint_DrawRectangle(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,
                         uint16_t Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill)
{
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
        Xend > Paint.Width || Yend > Paint.Height) {
        Debug("Input exceeds the normal display range\r\n");
        return;
    }

    if (Draw_Fill) {
        uint16_t Ypoint;
        for(Ypoint = Ystart; Ypoint < Yend; Ypoint++) {
            Paint_DrawLine(Xstart, Ypoint, Xend, Ypoint, Color , Line_width, LINE_STYLE_SOLID);
        }
    } else {
        Paint_DrawLine(Xstart, Ystart, Xend, Ystart, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xstart, Ystart, Xstart, Yend, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xend, Yend, Xend, Ystart, Color, Line_width, LINE_STYLE_SOLID);
        Paint_DrawLine(Xend, Yend, Xstart, Yend, Color, Line_width, LINE_STYLE_SOLID);
    }
}

/******************************************************************************
function: Use the 8-point method to draw a circle of the
            specified size at the specified position->
parameter:
    X_Center  ：Center X coordinate
    Y_Center  ：Center Y coordinate
    Radius    ：circle Radius
    Color     ：The color of the ：circle segment
    Line_width: Line width
    Draw_Fill : Whether to fill the inside of the Circle
******************************************************************************/
// static void Paint_DrawCircle(uint16_t X_Center, uint16_t Y_Center, uint16_t Radius,
//                       uint16_t Color, DOT_PIXEL Line_width, DRAW_FILL Draw_Fill)
// {
//     if (X_Center > Paint.Width || Y_Center >= Paint.Height) {
//         Debug("Paint_DrawCircle Input exceeds the normal display range\r\n");
//         return;
//     }

//     //Draw a circle from(0, R) as a starting point
//     int16_t XCurrent, YCurrent;
//     XCurrent = 0;
//     YCurrent = Radius;

//     //Cumulative error,judge the next point of the logo
//     int16_t Esp = 3 - (Radius << 1 );

//     int16_t sCountY;
//     if (Draw_Fill == DRAW_FILL_FULL) {
//         while (XCurrent <= YCurrent ) { //Realistic circles
//             for (sCountY = XCurrent; sCountY <= YCurrent; sCountY ++ ) {
//                 Paint_DrawPoint(X_Center + XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//1
//                 Paint_DrawPoint(X_Center - XCurrent, Y_Center + sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//2
//                 Paint_DrawPoint(X_Center - sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//3
//                 Paint_DrawPoint(X_Center - sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//4
//                 Paint_DrawPoint(X_Center - XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//5
//                 Paint_DrawPoint(X_Center + XCurrent, Y_Center - sCountY, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//6
//                 Paint_DrawPoint(X_Center + sCountY, Y_Center - XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);//7
//                 Paint_DrawPoint(X_Center + sCountY, Y_Center + XCurrent, Color, DOT_PIXEL_DFT, DOT_STYLE_DFT);
//             }
//             if (Esp < 0 )
//                 Esp += 4 * XCurrent + 6;
//             else {
//                 Esp += 10 + 4 * (XCurrent - YCurrent );
//                 YCurrent --;
//             }
//             XCurrent ++;
//         }
//     } else { //Draw a hollow circle
//         while (XCurrent <= YCurrent ) {
//             Paint_DrawPoint(X_Center + XCurrent, Y_Center + YCurrent, Color, Line_width, DOT_STYLE_DFT);//1
//             Paint_DrawPoint(X_Center - XCurrent, Y_Center + YCurrent, Color, Line_width, DOT_STYLE_DFT);//2
//             Paint_DrawPoint(X_Center - YCurrent, Y_Center + XCurrent, Color, Line_width, DOT_STYLE_DFT);//3
//             Paint_DrawPoint(X_Center - YCurrent, Y_Center - XCurrent, Color, Line_width, DOT_STYLE_DFT);//4
//             Paint_DrawPoint(X_Center - XCurrent, Y_Center - YCurrent, Color, Line_width, DOT_STYLE_DFT);//5
//             Paint_DrawPoint(X_Center + XCurrent, Y_Center - YCurrent, Color, Line_width, DOT_STYLE_DFT);//6
//             Paint_DrawPoint(X_Center + YCurrent, Y_Center - XCurrent, Color, Line_width, DOT_STYLE_DFT);//7
//             Paint_DrawPoint(X_Center + YCurrent, Y_Center + XCurrent, Color, Line_width, DOT_STYLE_DFT);//0

//             if (Esp < 0 )
//                 Esp += 4 * XCurrent + 6;
//             else {
//                 Esp += 10 + 4 * (XCurrent - YCurrent );
//                 YCurrent --;
//             }
//             XCurrent ++;
//         }
//     }
// }

/******************************************************************************
function: Show English characters
parameter:
    Xpoint           ：X coordinate
    Ypoint           ：Y coordinate
    Acsii_Char       ：To display the English characters
    Font             ：A structure pointer that displays a character size
    Color_Foreground : Select the foreground color
    Color_Background : Select the background color
******************************************************************************/
static void Paint_DrawChar(uint16_t Xpoint, uint16_t Ypoint, const char Acsii_Char,
                    sFONT* Font, uint16_t Color_Foreground, uint16_t Color_Background)
{
    uint16_t Page, Column;

    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DrawChar Input exceeds the normal display range\r\n");
        return;
    }

    uint32_t Char_Offset = (Acsii_Char - ' ') * Font->Height * (Font->Width / 8 + (Font->Width % 8 ? 1 : 0));
    const unsigned char *ptr = &Font->table[Char_Offset];

    for (Page = 0; Page < Font->Height; Page ++ ) {
        for (Column = 0; Column < Font->Width; Column ++ ) {

            //To determine whether the font background color and screen background color is consistent
            if (FONT_BACKGROUND == Color_Background) { //this process is to speed up the scan
                if (*ptr & (0x80 >> (Column % 8)))
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Foreground);
                    // Paint_DrawPoint(Xpoint + Column, Ypoint + Page, Color_Foreground, DOT_PIXEL_DFT, DOT_STYLE_DFT);
            } else {
                if (*ptr & (0x80 >> (Column % 8))) {
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Foreground);
                    // Paint_DrawPoint(Xpoint + Column, Ypoint + Page, Color_Foreground, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                } else {
                    Paint_SetPixel(Xpoint + Column, Ypoint + Page, Color_Background);
                    // Paint_DrawPoint(Xpoint + Column, Ypoint + Page, Color_Background, DOT_PIXEL_DFT, DOT_STYLE_DFT);
                }
            }
            //One pixel is 8 bits
            if (Column % 8 == 7)
                ptr++;
        }// Write a line
        if (Font->Width % 8 != 0)
            ptr++;
    }// Write all
}

/******************************************************************************
function:	Display the string
parameter:
    Xstart           ：X coordinate
    Ystart           ：Y coordinate
    pString          ：The first address of the English string to be displayed
    Font             ：A structure pointer that displays a character size
    Color_Foreground : Select the foreground color
    Color_Background : Select the background color
******************************************************************************/
static void Paint_DrawString_EN(uint16_t Xstart, uint16_t Ystart, const char * pString,
                         sFONT* Font, uint16_t Color_Foreground, uint16_t Color_Background)
{
    uint16_t Xpoint = Xstart;
    uint16_t Ypoint = Ystart;

    if (Xstart > Paint.Width || Ystart > Paint.Height) {
        Debug("Paint_DrawString_EN Input exceeds the normal display range\r\n");
        return;
    }

    while (* pString != '\0') {
        //if X direction filled , reposition to(Xstart,Ypoint),Ypoint is Y direction plus the Height of the character
        if ((Xpoint + Font->Width ) > Paint.Width ) {
            Xpoint = Xstart;
            Ypoint += Font->Height;
        }

        // If the Y direction is full, reposition to(Xstart, Ystart)
        if ((Ypoint  + Font->Height ) > Paint.Height ) {
            Xpoint = Xstart;
            Ypoint = Ystart;
        }
        Paint_DrawChar(Xpoint, Ypoint, * pString, Font, Color_Background, Color_Foreground);

        //The next character of the address
        pString ++;

        //The next word of the abscissa increases the font of the broadband
        Xpoint += Font->Width;
    }
}

/******************************************************************************
function:	Display nummber
parameter:
    Xstart           ：X coordinate
    Ystart           : Y coordinate
    Nummber          : The number displayed
    Font             ：A structure pointer that displays a character size
    Color_Foreground : Select the foreground color
    Color_Background : Select the background color
******************************************************************************/
#define  ARRAY_LEN 255
static void Paint_DrawNum(uint16_t Xpoint, uint16_t Ypoint, int32_t Nummber,
                   sFONT* Font, uint16_t Color_Foreground, uint16_t Color_Background)
{

    int16_t Num_Bit = 0, Str_Bit = 0;
    uint8_t Str_Array[ARRAY_LEN] = {0}, Num_Array[ARRAY_LEN] = {0};
    uint8_t *pStr = Str_Array;

    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DisNum Input exceeds the normal display range\r\n");
        return;
    }

    //Converts a number to a string
    do {
        Num_Array[Num_Bit] = Nummber % 10 + '0';
        Num_Bit++;
        Nummber /= 10;
    } while(Nummber);
    

    //The string is inverted
    while (Num_Bit > 0) {
        Str_Array[Str_Bit] = Num_Array[Num_Bit - 1];
        Str_Bit ++;
        Num_Bit --;
    }

    //show
    Paint_DrawString_EN(Xpoint, Ypoint, (const char*)pStr, Font, Color_Background, Color_Foreground);
}

/******************************************************************************
function:	Display nummber (Able to display decimals)
parameter:
    Xstart           ：X coordinate
    Ystart           : Y coordinate
    Nummber          : The number displayed
    Font             ：A structure pointer that displays a character size
    Digit            : Fractional width
    Color_Foreground : Select the foreground color
    Color_Background : Select the background color
******************************************************************************/
static void Paint_DrawNumDecimals(uint16_t Xpoint, uint16_t Ypoint, double Nummber,
                    sFONT* Font, uint16_t Digit, uint16_t Color_Foreground, uint16_t Color_Background)
{
    int16_t Num_Bit = 0, Str_Bit = 0;
    uint8_t Str_Array[ARRAY_LEN] = {0}, Num_Array[ARRAY_LEN] = {0};
    uint8_t *pStr = Str_Array;
	int temp = Nummber;
	float decimals;
	uint8_t i;
    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DisNum Input exceeds the normal display range\r\n");
        return;
    }

	if(Digit > 0) {		
		decimals = Nummber - temp;
		for(i=Digit; i > 0; i--) {
			decimals*=10;
		}
		temp = decimals;
		//Converts a number to a string
		for(i=Digit; i>0; i--) {
			Num_Array[Num_Bit] = temp % 10 + '0';
			Num_Bit++;
			temp /= 10;						
		}	
		Num_Array[Num_Bit] = '.';
		Num_Bit++;
	}

	temp = Nummber;
    //Converts a number to a string
    do {
        Num_Array[Num_Bit] = temp % 10 + '0';
        Num_Bit++;
        temp /= 10;
    } while(temp);

    //The string is inverted
    while (Num_Bit > 0) {
        Str_Array[Str_Bit] = Num_Array[Num_Bit - 1];
        Str_Bit ++;
        Num_Bit --;
    }

    //show
    Paint_DrawString_EN(Xpoint, Ypoint, (const char*)pStr, Font, Color_Background, Color_Foreground);
}

/******************************************************************************
function:	Display time
parameter:
    Xstart           ：X coordinate
    Ystart           : Y coordinate
    pTime            : Time-related structures
    Font             ：A structure pointer that displays a character size
    Color_Foreground : Select the foreground color
    Color_Background : Select the background color
******************************************************************************/
// static void Paint_DrawTime(uint16_t Xstart, uint16_t Ystart, PAINT_TIME *pTime, sFONT* Font,
//                     uint16_t Color_Foreground, uint16_t Color_Background)
// {
//     uint8_t value[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

//     uint16_t Dx = Font->Width;

//     //Write data into the cache
//     Paint_DrawChar(Xstart                           , Ystart, value[pTime->Hour / 10], Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx                      , Ystart, value[pTime->Hour % 10], Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx  + Dx / 4 + Dx / 2   , Ystart, ':'                    , Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx * 2 + Dx / 2         , Ystart, value[pTime->Min / 10] , Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx * 3 + Dx / 2         , Ystart, value[pTime->Min % 10] , Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx * 4 + Dx / 2 - Dx / 4, Ystart, ':'                    , Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx * 5                  , Ystart, value[pTime->Sec / 10] , Font, Color_Background, Color_Foreground);
//     Paint_DrawChar(Xstart + Dx * 6                  , Ystart, value[pTime->Sec % 10] , Font, Color_Background, Color_Foreground);
// }

/******************************************************************************
function:	Display monochrome bitmap
parameter:
    image_buffer ：A picture data converted to a bitmap
info:
    Use a computer to convert the image into a corresponding array,
    and then embed the array directly into Imagedata.cpp as a .c file.
******************************************************************************/
// static void Paint_DrawBitMap(const unsigned char* image_buffer)
// {
//     uint16_t x, y;
//     uint32_t Addr = 0;

//     for (y = 0; y < Paint.HeightByte; y++) {
//         for (x = 0; x < Paint.WidthByte; x++) {//8 pixel =  1 byte
//             Addr = x + y * Paint.WidthByte;
//             Paint.Image[Addr] = (unsigned char)image_buffer[Addr];
//         }
//     }
// }

/******************************************************************************
function:	paste monochrome bitmap to a frame buff
parameter:
    image_buffer ：A picture data converted to a bitmap
    xStart: The starting x coordinate
    yStart: The starting y coordinate
    imageWidth: Original image width
    imageHeight: Original image height
    flipColor: Whether the color is reversed
info:
    Use this function to paste image data into a buffer
******************************************************************************/
// static void Paint_DrawBitMap_Paste(const unsigned char* image_buffer, uint16_t xStart, uint16_t yStart, uint16_t imageWidth, uint16_t imageHeight, uint8_t flipColor)
// {
//     uint8_t color, srcImage;
//     uint16_t x, y;
//     uint16_t width = (imageWidth%8==0 ? imageWidth/8 : imageWidth/8+1);
    
//     for (y = 0; y < imageHeight; y++) {
//         for (x = 0; x < imageWidth; x++) {
//             srcImage = image_buffer[y*width + x/8];
//             if(flipColor)
//                 color = (((srcImage<<(x%8) & 0x80) == 0) ? 1 : 0);
//             else
//                 color = (((srcImage<<(x%8) & 0x80) == 0) ? 0 : 1);
//             Paint_SetPixel(x+xStart, y+yStart, color);
//         }
//     }
// }


// static void Paint_DrawBitMap_Block(const unsigned char* image_buffer, uint8_t Region)
// {
//     uint16_t x, y;
//     uint32_t Addr = 0;
// 		for (y = 0; y < Paint.HeightByte; y++) {
// 				for (x = 0; x < Paint.WidthByte; x++) {//8 pixel =  1 byte
// 						Addr = x + y * Paint.WidthByte ;
// 						Paint.Image[Addr] =  (unsigned char)image_buffer[Addr+ (Paint.HeightByte)*Paint.WidthByte*(Region - 1)];
// 				}
// 		}
// }

static void Paint_DrawCenteredLabeledFloat(float data, char label, uint32_t maxCharWidth, sFONT *font, uint32_t startX, uint32_t startY)
{
    uint32_t maxStrLength = EPD_HEIGHT / font->Width;
    if((maxCharWidth <= 0) || (maxCharWidth > maxStrLength))
    {
        Debug("Requested character width invalid!");
        return;
    }
    
    Paint_ClearWindows(startX, startY, startX + font->Width * maxCharWidth, startY + font->Height, WHITE);

    // Determine the max and min printable values
    int32_t maxData = 0;
    for(uint32_t i = 0; i < (maxCharWidth - 1); i++)
    {
        maxData = (maxData * 10) + 9;
    }
    int32_t minData = -1 * (maxData / 10);

    // Wrap to printable bounds
    if(data > (float)maxData)
    {
        data = (float)maxData;
    }
    else if(data < (float)minData)
    {
        data = (float)minData;
    }

    uint8_t pStr[MAX_PRINT_LENGTH] = {0}; 
    uint32_t digits = 0;

    // Determine if data is negative and flip sign if neccesary
    // Minus sign will take up one digit space
    if(data < 0)
    {
        pStr[digits] = '-';
        digits++;
        data *= -1;
    }

    uint32_t wholeNum = (uint32_t) data;
    uint8_t tempStr[MAX_PRINT_LENGTH] = {0};
    uint8_t tempDigits = 0;
	while((wholeNum > 0) && ((digits + tempDigits + 1) < maxCharWidth))
	{
        tempStr[tempDigits] = (wholeNum % 10) + '0';
        tempDigits++;
        wholeNum /= 10;
	}
    for(int32_t i = 1; i <= tempDigits; i++)
    {
        pStr[digits] = tempStr[tempDigits - i];
        digits++;
    }

    if((digits + 3) <= maxCharWidth)
    {
        pStr[digits] = '.';
		digits++;

        float temp = data;
        while((digits + 1) < maxCharWidth)
        {
            temp *= 10;
            pStr[digits] = ((uint32_t)temp % 10) + '0';
            digits++;
        }
    }

    pStr[digits] = label;
    digits++;

    if(digits < maxCharWidth)
    {
        startX += ((maxCharWidth - digits) * font->Width) / 2;
    }

    Paint_DrawString_EN(startX, startY, (const char*)pStr, font, WHITE, BLACK);
}

/*!
  @brief    Calculate number of integer digits in a number
  @param    num Target number to calculate number of digits for
*/
static uint8_t calculateIntegerDigits(uint32_t num)
{
    uint8_t digits = 1;
	while((num /= 10) > 0)
	{
		digits++;
	}
    return digits;
}

/* ==================================================================== */
/* =================== GLOBAL FUNCTION DEFINITIONS ==================== */
/* ==================================================================== */

/*!
  @brief    Initialize BMS template image
  @param    emptyImage Empty image array to modify with BMS data 
*/
void Paint_InitBmsImage(uint8_t* emptyImage)
{
    // Initilize bms image array as a blank image
	Paint_NewImage(emptyImage, EPD_WIDTH, EPD_HEIGHT, 90, WHITE);
	Paint_Clear(WHITE);

	// Create data table template
	Paint_DrawRectangle(39, 60, 239, 123, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
	Paint_DrawLine(39, 81, 237, 81, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
	Paint_DrawLine(39, 102, 237, 102, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
	Paint_DrawLine(113, 60, 113, 123, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
	Paint_DrawLine(176, 60, 176, 123, BLACK, DOT_PIXEL_2X2, LINE_STYLE_SOLID);

	// Print table labels for pack voltage, pack temp, and board temp
	Paint_DrawString_EN(54, 43, "VOLT", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(122, 43, "TEMP", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(180, 43, "BLEED", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(252, 43, "SOC", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(2, 63, "AVG", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(2, 84, "MAX", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(2, 105, "MIN", &Font16, WHITE, BLACK);

	// Print labels for State and Fault indicators
	Paint_DrawString_EN(2, 5, "STATE:", &Font16, WHITE, BLACK);
	Paint_DrawString_EN(2, 25, "FAULT:", &Font16, WHITE, BLACK);

	// Print default battery template
	Paint_DrawRectangle(255, 65, 283, 102, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
	Paint_DrawRectangle(263, 60, 275, 65, BLACK, DOT_PIXEL_2X2, DRAW_FILL_EMPTY);
}

/*!
  @brief    Update BMS Image with current voltage, pack temp, and board temp data
  @param    data Data to display on table
  @param    col Column of data table to populate
  @param    row Row of data table to populate 
*/
void Paint_DrawTableData(float data, DATA_TABLE_COL col, DATA_TABLE_ROW row)
{
    // Set starting X and Y cordinates
    uint16_t startX = DATA_START_X;
    for(int32_t i = 0; i < col; i++)
    {
        startX += tableSpacingCol[i];
    }
    uint16_t startY = DATA_START_Y;
    for(int32_t i = 0; i < row; i++)
    {
        startY += tableSpacingRow[i];
    }

    // Set label
    uint8_t label;
    uint32_t printWidth;
    if(col == DATA_VOLTAGE)
    {
        label = 'V';
        printWidth = 6;
    }
    else
    {
        label = 'C';
        printWidth = 5;
    }

    Paint_DrawCenteredLabeledFloat(data, label, printWidth, &Font16, startX, startY);
}

/*!
  @brief    Update BMS Image with current SOC
  @param    SOC BMS State of Charge as a percentage 
*/
void Paint_DrawSOC(uint32_t SOC)
{
	// Check bounds of SOC
	if(SOC < 0)
	{
		SOC = 0;
	}
	else if(SOC > 100)
	{
		SOC = 100;
	}

	// Calculate the number of digits in the SOC percent value
	uint8_t digits = calculateIntegerDigits(SOC);

	// The x value in the below statements is slected such that 100% is centered
	// In order to center the SOC percentage given any number of digits, 
	// the printed data is shifted by half a width for each digit less than three (100) 
	uint16_t startXAdjust =  (Font16.Width / 2) * (3 - digits);

	// Populate BMS image with current SOC percent
	Paint_ClearWindows(246 + startXAdjust, 105, 246 + Font16.Width * 4, 105 + Font16.Height, WHITE);
	Paint_DrawNum(246 + startXAdjust, 105, SOC, &Font16, BLACK, WHITE);

	// Place a percent sign directly after the last number, given by the font width times the number of digits
	Paint_DrawString_EN(246 + startXAdjust + Font16.Width * digits, 105, "%", &Font16, WHITE, BLACK);

	// Adjust the height of the printed battery level based on the SOC as a percentage
    uint16_t startSOCX = 102 - ((31 * SOC) / 100);
	// Paint_DrawRectangle(255, 102 - ((37 * SOC) / 100), 283, 102, BLACK, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    // Paint_DrawRectangle(255, 93, 283, 102, BLACK, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawRectangle(255, startSOCX, 283, 102, BLACK, DOT_PIXEL_2X2, DRAW_FILL_FULL);

    static int32_t startIndex = 0;
    uint32_t mask = 1;
    for(int32_t i = 0; i < (25-startIndex); i++)
    {
        Paint_DrawPoint(257+i+startIndex, startSOCX-2, (waveImage[0] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1);
        Paint_DrawPoint(257+i+startIndex, startSOCX-3, (waveImage[1] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1);
        mask <<= 1;
    }
    for(int32_t i = 0; i < startIndex; i++)
    { 
        Paint_DrawPoint(257+i, startSOCX-2, (waveImage[0] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1);
        Paint_DrawPoint(257+i, startSOCX-3, (waveImage[1] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1);
        mask <<= 1;
    }
    startIndex += 2;

    if(startIndex >= 25)
    {
        startIndex = 0;
    }
}

/*!
  @brief   Update BMS Image with state data
*/
void Paint_DrawState(char* stateMessage)
{
	Paint_ClearWindows(67, 5, 70 + Font16.Width * 12, 5 + Font16.Height, WHITE);
	Paint_DrawString_EN(67, 5, stateMessage, &Font16, WHITE, BLACK);
}

/*!
  @brief   Update BMS Image with fault data
*/
void Paint_DrawFault(char* faultMessage)
{
    Paint_ClearWindows(67, 25, 70 + Font16.Width * 12, 25 + Font16.Height, WHITE);
	Paint_DrawString_EN(67, 25, faultMessage, &Font16, WHITE, BLACK);
}

/*!
  @brief   Update BMS Image with current sensor data
*/
void Paint_DrawCurrent(float current)
{
    Paint_DrawCenteredLabeledFloat(current, 'A', 4, &Font16, 246, 5);

    Paint_ClearWindows(252, 25, 285, 40, WHITE);
    if(current > 0)
    {
        Paint_DrawLine(268, 29, 268, 36, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    }
    else if(current < 0)
    {

    }
	
}
