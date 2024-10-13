#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_log.h"
#include <optional>
#include <vector>
#include <iostream>
#include "hdlc.h"
#include "midi.h"
#include "tonex.h"

static const uart_port_t UART_PORT_NUM = UART_NUM_1;
static const uint8_t MIDI_CHANNEL = 3;
static const int BUF_SIZE = 1024;

std::optional<ProgramChange> parseProgramChange(const uint8_t *buffer, size_t bufferSize)
{
    for (size_t i = 0; i < bufferSize; ++i)
    {
        // Check if this byte is a status byte for Program Change
        if ((buffer[i] & 0xF0) == 0xC0)
        {
            // Program Change status byte found
            uint8_t channel = buffer[i] & 0x0F;

            // Ensure there's a data byte following the status byte
            if (i + 1 < bufferSize)
            {
                uint8_t programNumber = buffer[i + 1];
                // Print the parsed information
                printf("Program Change detected:\n");
                printf("  Channel: %d\n", channel + 1);
                printf("  Program Number: %d\n", programNumber);

                // Print raw message as hexadecimal string
                printf("  Raw message: ");
                // printHexString(&buffer[i], 2); // Print status byte and data byte
                return std::optional<ProgramChange>{{channel, programNumber}};
                // Skip the data byte
                //++i;
            }
            else
            {
                printf("Warning: Incomplete Program Change message at end of buffer\n");
                printf("  Raw message: ");
                // printHexString(&buffer[i], 1); // Print only the status byte
            }
        }
    }
    return std::nullopt;
}

void midi_receiver(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 31250,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, 4, 5, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);

    while (1)
    {
        int len = uart_read_bytes(UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        if (len)
        {
            auto programChange = parseProgramChange(data, len);
            if (programChange && programChange->channel == MIDI_CHANNEL)
            {
                auto slotNumber = programChange->programNumber == 0 ? Slot::B : Slot::A;
                //tonex.setSlot(slotNumber);
            }
        }
    }
}