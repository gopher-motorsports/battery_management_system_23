#include "main.h"
#include "epaperTask.h"
#include "EPD_Test.h"
#include <stdbool.h>

bool init = true;

void runEpaper()
{
    if(init)
    {
        epdClear();
        init = false;
    }
    else
    {

    }
}