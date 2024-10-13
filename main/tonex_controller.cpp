#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi.h"
#include "usb.h"
#include "tonex.h"

extern "C" void app_main(void)
{
    // xTaskCreate(usb_host_task, "usb_host_task", 4096, NULL, 20, NULL);
    Tonex tonex;
    tonex.init();
    xTaskCreate(midi_receiver, "midi_receiver", 2048, &tonex, 10, NULL);

    while(1) {}
}
