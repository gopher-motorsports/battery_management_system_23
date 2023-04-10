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

PAINT Paint;

uint32_t waveImage[2] =
{
    0b00000001000000000011100000000001,
    0b00000001110000001111111000000111
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
		
    Paint.WidthByte = (Width % 8 == 0)? (Width / 8 ): (Width / 8 + 1);
    Paint.HeightByte = Height;    
//    printf("WidthByte = %d, HeightByte = %d\r\n", Paint.WidthByte, Paint.HeightByte);
//    printf(" EPD_WIDTH / 8 = %d\r\n",  122 / 8);
   
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

/******************************************************************************
function: Select Image
parameter:
    image : Pointer to the image cache
******************************************************************************/
// static void Paint_SelectImage(uint8_t *image)
// {
//     Paint.Image = image;
// }

/******************************************************************************
function: Select Image Rotate
parameter:
    Rotate : 0,90,180,270
******************************************************************************/
// static void Paint_SetRotate(uint16_t Rotate)
// {
//     if(Rotate == ROTATE_0 || Rotate == ROTATE_90 || Rotate == ROTATE_180 || Rotate == ROTATE_270) {
//         Debug("Set image Rotate %d\r\n", Rotate);
//         Paint.Rotate = Rotate;
//     } else {
//         Debug("rotate = 0, 90, 180, 270\r\n");
//     }
// }

// static void Paint_SetScale(uint8_t scale)
// {
//     if(scale == 2){
//         Paint.Scale = scale;
//         Paint.WidthByte = (Paint.WidthMemory % 8 == 0)? (Paint.WidthMemory / 8 ): (Paint.WidthMemory / 8 + 1);
//     }else if(scale == 4){
//         Paint.Scale = scale;
//         Paint.WidthByte = (Paint.WidthMemory % 4 == 0)? (Paint.WidthMemory / 4 ): (Paint.WidthMemory / 4 + 1);
//     }else if(scale == 7){//Only applicable with 5in65 e-Paper
// 				Paint.Scale = scale;
// 				Paint.WidthByte = (Paint.WidthMemory % 2 == 0)? (Paint.WidthMemory / 2 ): (Paint.WidthMemory / 2 + 1);;
// 		}else{
//         Debug("Set Scale Input parameter error\r\n");
//         Debug("Scale Only support: 2 4 7\r\n");
//     }
// }
/******************************************************************************
function:	Select Image mirror
parameter:
    mirror   :Not mirror,Horizontal mirror,Vertical mirror,Origin mirror
******************************************************************************/
// static void Paint_SetMirroring(uint8_t mirror)
// {
//     if(mirror == MIRROR_NONE || mirror == MIRROR_HORIZONTAL || 
//         mirror == MIRROR_VERTICAL || mirror == MIRROR_ORIGIN) {
//         Debug("mirror image x:%s, y:%s\r\n",(mirror & 0x01)? "mirror":"none", ((mirror >> 1) & 0x01)? "mirror":"none");
//         Paint.Mirror = mirror;
//     } else {
//         Debug("mirror should be MIRROR_NONE, MIRROR_HORIZONTAL, MIRROR_VERTICAL or MIRROR_ORIGIN\r\n");
//     }    
// }

/******************************************************************************
function: Draw Pixels
parameter:
    Xpoint : At point X
    Ypoint : At point Y
    Color  : Painted colors
******************************************************************************/
static void Paint_SetPixel(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color)
{
    if(Xpoint > Paint.Width || Ypoint > Paint.Height){
        Debug("Exceeding display boundaries\r\n");
        return;
    }      
    uint16_t X, Y;

    switch(Paint.Rotate) {
    case 0:
        X = Xpoint;
        Y = Ypoint;  
        break;
    case 90:
        X = Paint.WidthMemory - Ypoint - 1;
        Y = Xpoint;
        break;
    case 180:
        X = Paint.WidthMemory - Xpoint - 1;
        Y = Paint.HeightMemory - Ypoint - 1;
        break;
    case 270:
        X = Ypoint;
        Y = Paint.HeightMemory - Xpoint - 1;
        break;
    default:
        return;
    }
    
    switch(Paint.Mirror) {
    case MIRROR_NONE:
        break;
    case MIRROR_HORIZONTAL:
        X = Paint.WidthMemory - X - 1;
        break;
    case MIRROR_VERTICAL:
        Y = Paint.HeightMemory - Y - 1;
        break;
    case MIRROR_ORIGIN:
        X = Paint.WidthMemory - X - 1;
        Y = Paint.HeightMemory - Y - 1;
        break;
    default:
        return;
    }

    if(X > Paint.WidthMemory || Y > Paint.HeightMemory){
        Debug("Exceeding display boundaries\r\n");
        return;
    }
    
    if(Paint.Scale == 2){
        uint32_t Addr = X / 8 + Y * Paint.WidthByte;
        uint8_t Rdata = Paint.Image[Addr];
        if(Color == BLACK)
            Paint.Image[Addr] = Rdata & ~(0x80 >> (X % 8));
        else
            Paint.Image[Addr] = Rdata | (0x80 >> (X % 8));
    }else if(Paint.Scale == 4){
        uint32_t Addr = X / 4 + Y * Paint.WidthByte;
        Color = Color % 4;//Guaranteed color scale is 4  --- 0~3
        uint8_t Rdata = Paint.Image[Addr];
        
        Rdata = Rdata & (~(0xC0 >> ((X % 4)*2)));
        Paint.Image[Addr] = Rdata | ((Color << 6) >> ((X % 4)*2));
    }else if(Paint.Scale == 7){
		uint32_t Addr = X / 2  + Y * Paint.WidthByte;
		uint8_t Rdata = Paint.Image[Addr];
		Rdata = Rdata & (~(0xF0 >> ((X % 2)*4)));//Clear first, then set value
		Paint.Image[Addr] = Rdata | ((Color << 4) >> ((X % 2)*4));
		//printf("Add =  %d ,data = %d\r\n",Addr,Rdata);
		}
}

/******************************************************************************
function: Clear the color of the picture
parameter:
    Color : Painted colors
******************************************************************************/
static void Paint_Clear(uint16_t Color)
{
	if(Paint.Scale == 2) {
		for (uint16_t Y = 0; Y < Paint.HeightByte; Y++) {
			for (uint16_t X = 0; X < Paint.WidthByte; X++ ) {//8 pixel =  1 byte
				uint32_t Addr = X + Y*Paint.WidthByte;
				Paint.Image[Addr] = Color;
			}
		}		
    }else if(Paint.Scale == 4) {
        for (uint16_t Y = 0; Y < Paint.HeightByte; Y++) {
			for (uint16_t X = 0; X < Paint.WidthByte; X++ ) {
				uint32_t Addr = X + Y*Paint.WidthByte;
				Paint.Image[Addr] = (Color<<6)|(Color<<4)|(Color<<2)|Color;
			}
		}		
	}else if(Paint.Scale == 7) {
		for (uint16_t Y = 0; Y < Paint.HeightByte; Y++) {
			for (uint16_t X = 0; X < Paint.WidthByte; X++ ) {
				uint32_t Addr = X + Y*Paint.WidthByte;
				Paint.Image[Addr] = (Color<<4)|Color;
			}
		}		
	}
}

/******************************************************************************
function: Clear the color of a window
parameter:
    Xstart : x starting point
    Ystart : Y starting point
    Xend   : x end point
    Yend   : y end point
    Color  : Painted colors
******************************************************************************/
static void Paint_ClearWindows(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend, uint16_t Color)
{
    uint16_t X, Y;
    for (Y = Ystart; Y < Yend; Y++) {
        for (X = Xstart; X < Xend; X++) {//8 pixel =  1 byte
            Paint_SetPixel(X, Y, Color);
        }
    }
}

/******************************************************************************
function: Draw Point(Xpoint, Ypoint) Fill the color
parameter:
    Xpoint		: The Xpoint coordinate of the point
    Ypoint		: The Ypoint coordinate of the point
    Color		: Painted color
    Dot_Pixel	: point size
    Dot_Style	: point Style
******************************************************************************/
static void Paint_DrawPoint(uint16_t Xpoint, uint16_t Ypoint, uint16_t Color,
                     DOT_PIXEL Dot_Pixel, DOT_STYLE Dot_Style)
{
    if (Xpoint > Paint.Width || Ypoint > Paint.Height) {
        Debug("Paint_DrawPoint Input exceeds the normal display range\r\n");
				printf("Xpoint = %d , Paint.Width = %d  \r\n ",Xpoint,Paint.Width);
				printf("Ypoint = %d , Paint.Height = %d  \r\n ",Ypoint,Paint.Height);
        return;
    }

    int16_t XDir_Num , YDir_Num;
    if (Dot_Style == DOT_FILL_AROUND) {
        for (XDir_Num = 0; XDir_Num < 2 * Dot_Pixel - 1; XDir_Num++) {
            for (YDir_Num = 0; YDir_Num < 2 * Dot_Pixel - 1; YDir_Num++) {
                if(Xpoint + XDir_Num - Dot_Pixel < 0 || Ypoint + YDir_Num - Dot_Pixel < 0)
                    break;
                // printf("x = %d, y = %d\r\n", Xpoint + XDir_Num - Dot_Pixel, Ypoint + YDir_Num - Dot_Pixel);
                Paint_SetPixel(Xpoint + XDir_Num - Dot_Pixel, Ypoint + YDir_Num - Dot_Pixel, Color);
            }
        }
    } else {
        for (XDir_Num = 0; XDir_Num <  Dot_Pixel; XDir_Num++) {
            for (YDir_Num = 0; YDir_Num <  Dot_Pixel; YDir_Num++) {
                Paint_SetPixel(Xpoint + XDir_Num - 1, Ypoint + YDir_Num - 1, Color);
            }
        }
    }
}

/******************************************************************************
function: Draw a line of arbitrary slope
parameter:
    Xstart ：Starting Xpoint point coordinates
    Ystart ：Starting Xpoint point coordinates
    Xend   ：End point Xpoint coordinate
    Yend   ：End point Ypoint coordinate
    Color  ：The color of the line segment
    Line_width : Line width
    Line_Style: Solid and dotted lines
******************************************************************************/
static void Paint_DrawLine(uint16_t Xstart, uint16_t Ystart, uint16_t Xend, uint16_t Yend,
                    uint16_t Color, DOT_PIXEL Line_width, LINE_STYLE Line_Style)
{
    if (Xstart > Paint.Width || Ystart > Paint.Height ||
        Xend > Paint.Width || Yend > Paint.Height) {
        Debug("Paint_DrawLine Input exceeds the normal display range\r\n");
        return;
    }

    uint16_t Xpoint = Xstart;
    uint16_t Ypoint = Ystart;
    int dx = (int)Xend - (int)Xstart >= 0 ? Xend - Xstart : Xstart - Xend;
    int dy = (int)Yend - (int)Ystart <= 0 ? Yend - Ystart : Ystart - Yend;

    // Increment direction, 1 is positive, -1 is counter;
    int XAddway = Xstart < Xend ? 1 : -1;
    int YAddway = Ystart < Yend ? 1 : -1;

    //Cumulative error
    int Esp = dx + dy;
    char Dotted_Len = 0;

    for (;;) {
        Dotted_Len++;
        //Painted dotted line, 2 point is really virtual
        if (Line_Style == LINE_STYLE_DOTTED && Dotted_Len % 3 == 0) {
            //Debug("LINE_DOTTED\r\n");
            Paint_DrawPoint(Xpoint, Ypoint, IMAGE_BACKGROUND, Line_width, DOT_STYLE_DFT);
            Dotted_Len = 0;
        } else {
            Paint_DrawPoint(Xpoint, Ypoint, Color, Line_width, DOT_STYLE_DFT);
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

/*!
  @brief    Calculate number of integer digits in a number
  @param    num Target number to calculate number of digits for
*/
static uint8_t calculateIntegerDigits(uint32_t num)
{
    uint8_t digits = 1;
    for(int32_t i = 0; i < 12; i++)
    {
        num /= 10;
        if(num > 0)
        {
            break;
        }
    }
	while(((num /= 10) > 0) && (digits < 11))
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

Paint_DrawCenteredLabeledValue(float data, uint32_t startX, uint32_t startY, uint32_t placeValues, sFONT *font)
{
    Paint_ClearWindows(startX, startY, startX + font->Width * placeValues, startY + font->Height, WHITE);

    // Data position adjustment variables
    uint8_t digits = 0;
    bool negative = 0;

    // Determine if data is negative and flip sign if neccesary
    // Minus sign will take up one digit space
    if(data < 0.0f)
    {
        negative = true;
        data *= -1;
        digits++;
    }

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
    uint16_t startX = DATA_START_X + (col * DATA_SEPERATION_X);
    uint16_t startY = DATA_START_Y + (row * DATA_SEPERATION_Y);

    // Clear data entry space
    if(col == DATA_VOLTAGE)
    {
        Paint_ClearWindows(startX, startY, startX + Font16.Width * 6, startY + Font16.Height, WHITE);
    }
    else
    {
        startX += Font16.Width;
        Paint_ClearWindows(startX, startY, startX + Font16.Width * 5, startY + Font16.Height, WHITE);
    }

    // Wrap to printable bounds
    if(data > 9999.0f)
    {
        data = 9999.0f;
    }
    else if(data < -999.0f)
    {
        data = -999.0f;
    }

    // Data position adjustment variables
    uint8_t digits = 0;
    bool negative = 0;

    // Determine if data is negative and flip sign if neccesary
    // Minus sign will take up one digit space
    if(data < 0)
    {
        negative = true;
        data *= -1;
        digits++;
    }

    // Calucalte number of digits before decimal place
    digits += calculateIntegerDigits(data);

    bool decimal = false;
    if(digits < 3)
    {
        decimal = true;
        (col == DATA_VOLTAGE) ? (digits += 3) : (digits += 2);
    }

    startX += (Font16.Width / 2 ) * (4 - digits);

    if(negative)
    {
        Paint_DrawString_EN(startX, startY, "-", &Font16, WHITE, BLACK);
        startX += Font16.Width;
    }

    // Draw number as integer or decimal depending on number of digits
    if(decimal)
    {
        if(col == DATA_VOLTAGE)
        {
            Paint_DrawNumDecimals(startX, startY, data, &Font16, 3, BLACK, WHITE);
        }
        else
        {
            Paint_DrawNumDecimals(startX, startY, data, &Font16, 1, BLACK, WHITE);
        }
    }
    else
    {
        Paint_DrawNum(startX, startY, data, &Font16, BLACK, WHITE);
    }

    // Set data label to display
    if(col == DATA_VOLTAGE)
    {
        Paint_DrawString_EN(startX + ((digits+1) * Font16.Width) - (negative ? Font16.Width : 0), startY, "V", &Font16, WHITE, BLACK);
    }
    else
    {
        Paint_DrawString_EN(startX + (digits * Font16.Width) - (negative ? Font16.Width : 0), startY, "C", &Font16, WHITE, BLACK);
    }
	
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
        Paint_DrawPoint(257+i+startIndex, startSOCX-2, (waveImage[0] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
        Paint_DrawPoint(257+i+startIndex, startSOCX-3, (waveImage[1] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
        mask <<= 1;
    }
    for(int32_t i = 0; i < startIndex; i++)
    { 
        Paint_DrawPoint(257+i, startSOCX-2, (waveImage[0] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
        Paint_DrawPoint(257+i, startSOCX-3, (waveImage[1] & mask) ? WHITE : BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
        mask <<= 1;
    }
    startIndex += 2;

    if(startIndex >= 25)
    {
        startIndex = 0;
    }

    // Paint_DrawPoint(257, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(258, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(259, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(261, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(262, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(263, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(264, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(265, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(266, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(267, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(268, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(270, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(271, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(272, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(274, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(275, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(276, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(277, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(278, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(279, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(280, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

    // // Paint_DrawPoint(257, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(258, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(259, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(261, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(262, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(263, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(264, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(265, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(266, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(267, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(268, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(270, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(271, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(272, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(274, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(275, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(276, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(277, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(278, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(279, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(280, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

    // // Paint_DrawPoint(257, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(258, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(259, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(261, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(262, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(263, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(264, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(265, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(266, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(267, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(268, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(270, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(271, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(272, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(274, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(275, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(276, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(277, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(278, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(279, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(280, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-4, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

    // // Paint_DrawPoint(257, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(258, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(259, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(261, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(262, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(263, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(264, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(265, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(266, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(267, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(268, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(270, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(271, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(272, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(274, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(275, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(276, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(277, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(278, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(279, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(280, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-5, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

    // // Paint_DrawPoint(257, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(258, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(259, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(261, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(262, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(263, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(264, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(265, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(266, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(267, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(268, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(270, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(271, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(272, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(274, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(275, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(276, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(277, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(278, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(279, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(280, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-2, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

    // // Paint_DrawPoint(257, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(258, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(259, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(260, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(261, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(262, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(263, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(264, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(265, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(266, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(267, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(268, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(269, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(270, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(271, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(272, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(273, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(274, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(275, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(276, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(277, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // Paint_DrawPoint(278, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(279, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(280, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);
    // // Paint_DrawPoint(281, startSOCX-3, BLACK, DOT_PIXEL_1X1, DOT_FILL_AROUND);

}

/*!
  @brief   Update BMS Image with state data
*/
void Paint_DrawState()
{
	Paint_ClearWindows(67, 5, 70 + Font16.Width * 12, 5 + Font16.Height, WHITE);
	Paint_DrawString_EN(67, 5, "YOUR MOTHER", &Font16, WHITE, BLACK);
}

/*!
  @brief   Update BMS Image with fault data
*/
void Paint_DrawFault()
{
    Paint_ClearWindows(67, 25, 70 + Font16.Width * 12, 25 + Font16.Height, WHITE);
	Paint_DrawString_EN(67, 25, "NO FAULT", &Font16, WHITE, BLACK);
}