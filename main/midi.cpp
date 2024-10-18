/* 
 * MIT License
 *
 * Copyright (c) 2024 vit3k
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

namespace midi
{
    static const uart_port_t UART_PORT_NUM = UART_NUM_1;
    static const uint8_t MIDI_CHANNEL = 2;
    static const int BUF_SIZE = 128;
    static const char *TAG = "TONEX_CONTROLLER_MIDI";

    static std::vector<ProgramChange> parseProgramChanges(const uint8_t *buffer, size_t bufferSize);

    void midi_receiver(void *arg)
    {
        auto tonex = static_cast<Tonex *>(arg);
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

        auto data = new uint8_t[BUF_SIZE];

        while (1)
        {
            int len = uart_read_bytes(UART_PORT_NUM, data, (BUF_SIZE - 1), pdMS_TO_TICKS(20));
            if (len)
            {
                // ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);
                // ESP_LOGI(TAG, "Received %d bytes from UART", len);
                auto programChanges = parseProgramChanges(data, len);

                for (auto programChange : programChanges)
                {
                    if (programChange.channel != MIDI_CHANNEL)
                    {
                        continue;
                    }
                    auto slotNumber = programChange.programNumber == 1 ? Slot::B : Slot::A;
                    tonex->setSlot(slotNumber);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        delete data;
    }

    void init(Tonex *tonex)
    {
        xTaskCreatePinnedToCore(midi_receiver, "midi_receiver", 4096, tonex, 10, NULL, 0);
    }

    static std::vector<ProgramChange> parseProgramChanges(const uint8_t *buffer, size_t bufferSize)
    {
        std::vector<ProgramChange> programChanges;

        for (size_t i = 0; i < bufferSize; i++)
        {
            // Skip real-time messages (status bytes 0xF8 to 0xFF)
            if (buffer[i] >= 0xF8)
            {
                continue;
            }

            // Check if this byte is a status byte for Program Change
            if ((buffer[i] & 0xF0) == 0xC0)
            {
                // Program Change status byte found
                uint8_t channel = buffer[i] & 0x0F;

                // Ensure there's a data byte following the status byte
                if (i + 1 < bufferSize)
                {
                    uint8_t programNumber = buffer[i + 1];
                    ESP_LOGI(TAG, "Received program change [channel: %d, program: %d]", channel, programNumber);
                    programChanges.push_back({channel, programNumber});

                    // Skip the data byte
                    i++;
                }
                else
                {
                    ESP_LOGW(TAG, "Warning: Incomplete Program Change message at end of buffer");
                    break;
                }
            }
            else if (buffer[i] & 0x80)
            {
                // This is a status byte for a different type of message
                // Skip this message by finding the next status byte or end of buffer
                while (++i < bufferSize && !(buffer[i] & 0x80))
                {
                }
                i--; // Decrement i because the for loop will increment it again
            }
            // If it's not a status byte, it's a data byte of a message we're not interested in
            // The loop will automatically move to the next byte
        }

        return programChanges;
    }
}