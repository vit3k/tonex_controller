#include "usb.h"

#include "esp_log.h"
#include <vector>

static SemaphoreHandle_t device_disconnected_sem;



static const char *TAG = "TONEX_CONTROLLER";
/**
 * @brief Device event callback
 *
 * Apart from handling device disconnection it doesn't do anything useful
 *
 * @param[in] event    Device event type and data
 * @param[in] arg      Argument we passed to the device open function
 */
void USB::handle_event(const cdc_acm_host_dev_event_data_t *event, void *arg)
{
    auto usb = static_cast<USB*>(arg);
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        usb->connected = false;
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

/**
 * @brief USB Host library handling task
 *
 * @param arg Unused
 */
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

bool USB::handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    auto usb = static_cast<USB*>(arg);
    ESP_LOGI(TAG, "Data received");
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, data_len, ESP_LOG_INFO);
    std::vector<uint8_t> message(data, data + data_len);
    usb->onMessageCallback(message);
    return true;
}

void USB::usb_host_task(void* arg)
{
    auto usb = static_cast<USB*>(arg);
    device_disconnected_sem = xSemaphoreCreateBinary();
    assert(device_disconnected_sem);

    // Install USB Host driver. Should only be called once in entire application
    ESP_LOGI(TAG, "Installing USB Host");
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    // Create a task that will handle USB library events
    BaseType_t task_created = xTaskCreate(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 20, NULL);
    assert(task_created == pdTRUE);

    ESP_LOGI(TAG, "Installing CDC-ACM driver");
    ESP_ERROR_CHECK(cdc_acm_host_install(NULL));

    const cdc_acm_host_device_config_t dev_config = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = 512,
        .in_buffer_size = 512,
        .event_cb = USB::handle_event,
        .data_cb = USB::handle_rx,
        .user_arg = usb};

    while (true)
    {
        usb->cdc_dev = NULL;

        // Open USB device from tusb_serial_device example example. Either single or dual port configuration.
        ESP_LOGI(TAG, "Opening CDC ACM device 0x%04X:0x%04X...", usb->vid, usb->pid);
        esp_err_t err = cdc_acm_host_open(usb->vid, usb->pid, 0, &dev_config, &(usb->cdc_dev));
        if (ESP_OK != err)
        {
            ESP_LOGI(TAG, "Failed to open device");
            continue;
        }
        cdc_acm_host_desc_print(usb->cdc_dev);
        vTaskDelay(pdMS_TO_TICKS(100));
        usb->connected = true;
        xSemaphoreTake(device_disconnected_sem, portMAX_DELAY);
    }
}

void USB::send(const std::vector<uint8_t>& data)
{
    if (!connected) {
        return;
    }
    ESP_ERROR_CHECK(cdc_acm_host_data_tx_blocking(cdc_dev, data.data(), data.size(), 20));
    vTaskDelay(pdMS_TO_TICKS(100));
}

std::unique_ptr<USB> USB::init(uint16_t vid, uint16_t pid, std::function<void(const std::vector<uint8_t>&)> onMessageCallback)
{
    auto usb = new USB();
    usb->pid = pid;
    usb->vid = vid;
    usb->onMessageCallback = onMessageCallback;
    xTaskCreate(USB::usb_host_task, "usb_host_task", 4096, usb, 5, NULL);
    return std::unique_ptr<USB>(usb);
}
