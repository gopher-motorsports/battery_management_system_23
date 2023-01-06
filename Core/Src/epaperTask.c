#include "epaperTask.h"
#include "EPD_Test.h"
#include <stdbool.h>

bool init = true;

void runEpaper()
{
    if(init)
    {
        EPD_test();
        init = false;
    }
}