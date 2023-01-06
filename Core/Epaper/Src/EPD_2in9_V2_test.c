#include "EPD_Test.h"

int EPD_test(void)
{
    epdClear();

    //Create a new image cache
    uint8_t *BlackImage;
    uint16_t Imagesize = (EPD_WIDTH / 8 ) * EPD_HEIGHT;
    BlackImage = (uint8_t *)malloc(Imagesize);
    Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 90, WHITE);
	Paint_Clear(WHITE);

#if 1  // Basic Display Test
	Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 90, WHITE);  	
    Paint_SelectImage(BlackImage);
    Paint_Clear(WHITE);
	
    Paint_DrawPoint(10, 80, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 90, BLACK, DOT_PIXEL_2X2, DOT_STYLE_DFT);
    Paint_DrawPoint(10, 100, BLACK, DOT_PIXEL_3X3, DOT_STYLE_DFT);

    Paint_DrawLine(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    Paint_DrawLine(70, 70, 20, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);

    Paint_DrawRectangle(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawRectangle(80, 70, 130, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    Paint_DrawCircle(45, 95, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    Paint_DrawCircle(105, 95, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);

    Paint_DrawLine(85, 95, 125, 95, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);
    Paint_DrawLine(105, 75, 105, 115, BLACK, DOT_PIXEL_1X1, LINE_STYLE_DOTTED);

    Paint_DrawString_EN(10, 0, "waveshare", &Font16, BLACK, WHITE);
    Paint_DrawString_EN(10, 20, "hello world", &Font12, WHITE, BLACK);

    Paint_DrawNum(10, 33, 123456789, &Font12, BLACK, WHITE);
    Paint_DrawNum(10, 50, 987654321, &Font16, WHITE, BLACK);

    epdDisplay(BlackImage);
    vTaskDelay(3000 / portTICK_PERIOD_MS);
#endif


#if 0 // Bezos
    for(int i = 0; i < 128; i++)
    {
        for(int j = 0; j < 296; j++)
        {
            Paint_SetPixel(j, i, bezos[(296 * i) + j]);
        }
    }

    epdDisplay(BlackImage);
    vTaskDelay(3000 / portTICK_PERIOD_MS);

#endif



#if 0   //Partial refresh, example shows time    		
	Paint_NewImage(BlackImage, EPD_WIDTH, EPD_HEIGHT, 90, WHITE);  
    printf("Partial refresh\r\n");
    Paint_SelectImage(BlackImage);
	
    PAINT_TIME sPaint_time;
    sPaint_time.Hour = 12;
    sPaint_time.Min = 34;
    sPaint_time.Sec = 56;
    uint8_t num = 10;
    for (;;) {
        sPaint_time.Sec = sPaint_time.Sec + 1;
        if (sPaint_time.Sec == 60) {
            sPaint_time.Min = sPaint_time.Min + 1;
            sPaint_time.Sec = 0;
            if (sPaint_time.Min == 60) {
                sPaint_time.Hour =  sPaint_time.Hour + 1;
                sPaint_time.Min = 0;
                if (sPaint_time.Hour == 24) {
                    sPaint_time.Hour = 0;
                    sPaint_time.Min = 0;
                    sPaint_time.Sec = 0;
                }
            }
        }
        Paint_ClearWindows(150, 80, 150 + Font20.Width * 7, 80 + Font20.Height, WHITE);
        Paint_DrawTime(150, 80, &sPaint_time, &Font20, WHITE, BLACK);

        num = num - 1;
        if(num == 0) {
            break;
        }
		epdDisplayPartial(BlackImage);
        vTaskDelay(500 / portTICK_PERIOD_MS);//Analog clock 1s
    }
#endif

    epdClear();
    epdSleep();
    free(BlackImage);
    BlackImage = NULL;
    return 0;
}

