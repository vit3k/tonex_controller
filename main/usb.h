#pragma once 
#include <memory>
#include <functional>
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"

class USB {
private:
    cdc_acm_dev_hdl_t cdc_dev = nullptr;
    bool connected = false;
    std::function<void(const std::vector<uint8_t>&)> onMessageCallback;
    std::function<void(void)> onConnectionCallback;
    uint16_t vid;
    uint16_t pid;
    USB() = default;
public:
    static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *arg);
    static bool handle_rx(const uint8_t *data, size_t data_len, void *arg);
    static void usb_host_task(void* arg);
    static std::unique_ptr<USB> init(uint16_t vid, uint16_t pid, std::function<void(const std::vector<uint8_t>&)> onMessageCallback);
    void send(const std::vector<uint8_t>& data);
    void setConnectionCallback(std::function<void(void)> callback);
};
