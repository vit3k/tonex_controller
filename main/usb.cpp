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

#include "usb.h"

#include "esp_log.h"
#include <vector>
#include <numeric>
#include <hal/usb_dwc_hal.h>

static void usb_lib_task(void *arg);

static SemaphoreHandle_t device_disconnected_sem;

static const char *TAG = "TONEX_CONTROLLER_USB";

void USB::usb_host_task(void *arg)
{
    auto usb = static_cast<USB *>(arg);
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
        .fifo_settings_custom = {
            .nptx_fifo_lines = 128, // Must be > 0
            .ptx_fifo_lines = 32,  // Can be 0 if periodic TX endpoints are not used
            .rx_fifo_lines = 32    // Must be > 0
        },
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 1024,
        .in_buffer_size = 1024,
        .event_cb = USB::handle_event,
        .data_cb = USB::handle_rx,
        .user_arg = usb};

    while (true)
    {
        usb->connected = false;
        usb->cdc_dev = NULL;

        ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", usb->vid, usb->pid);
        esp_err_t err = cdc_acm_host_open(usb->vid, usb->pid, 0, &dev_config, &(usb->cdc_dev));
        if (ESP_OK != err)
        {
            ESP_LOGI(TAG, "Failed to open device");
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        cdc_acm_line_coding_t line_coding;
        line_coding.dwDTERate = 9600;
        line_coding.bDataBits = 8;
        line_coding.bParityType = 0;
        line_coding.bCharFormat = 1;
        ESP_ERROR_CHECK(cdc_acm_host_line_coding_set(usb->cdc_dev, &line_coding));

        ESP_ERROR_CHECK(cdc_acm_host_set_control_line_state(usb->cdc_dev, true, false));
        usb->connected = true;
        usb->onConnectionCallback();
        ESP_LOGI(TAG, "Connected");
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}


bool USB::handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    auto usb = static_cast<USB *>(arg);
    ESP_LOGI(TAG, "Data received");
    std::vector<uint8_t> message(data, data + data_len);
    usb->onMessageCallback(message);
    return true;
}


void USB::send(const std::vector<uint8_t> &data)
{
    if (!connected)
    {
        return;
    }
    ESP_ERROR_CHECK(cdc_acm_host_data_tx_blocking(cdc_dev, data.data(), data.size(), portMAX_DELAY));
    vTaskDelay(pdMS_TO_TICKS(100));
}

void USB::setConnectionCallback(std::function<void(void)> callback)
{
    onConnectionCallback = callback;
}

std::unique_ptr<USB> USB::init(uint16_t vid, uint16_t pid, std::function<void(const std::vector<uint8_t> &)> onMessageCallback)
{
    auto usb = new USB();
    usb->pid = pid;
    usb->vid = vid;
    usb->onMessageCallback = onMessageCallback;
    xTaskCreatePinnedToCore(USB::usb_host_task, "usb_host_task", 4096, usb, 5, NULL, 0);
    return std::unique_ptr<USB>(usb);
}

void USB::handle_event(const cdc_acm_host_dev_event_data_t *event, void *arg)
{
    // auto usb = static_cast<USB*>(arg);
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        xSemaphoreGive(device_disconnected_sem);
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

static void usb_lib_task(void *arg)
{
    while (1)
    {
        // Start handling system events
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
        {
            ESP_ERROR_CHECK(usb_host_device_free_all());
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE)
        {
            ESP_LOGI(TAG, "USB: All devices freed");
            // Continue handling USB events to allow device reconnection
        }
    }
}
