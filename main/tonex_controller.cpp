#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi.h"
#include "usb.h"
#include "tonex.h"
Tonex tonex;

extern "C" void app_main(void)
{   
    tonex.init();
    xTaskCreatePinnedToCore(midi_receiver, "midi_receiver", 4096, &tonex, 10, NULL, 0);
}