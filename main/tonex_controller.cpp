#include "midi.h"
#include "usb.h"
#include "tonex.h"

Tonex tonex;

extern "C" void app_main(void)
{   
    tonex.init();
    midi::init(&tonex);
}