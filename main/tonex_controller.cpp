#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "midi.h"
#include "usb.h"



extern "C" void app_main(void)
{
    xTaskCreate(usb_host_task, "usb_host_task", 4096, NULL, 20, NULL);
    xTaskCreate(midi_receiver, "midi_receiver", 2048, NULL, 10, NULL);
}
