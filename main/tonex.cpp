#include "tonex.h"
#include "esp_log.h"
#include "hdlc.h"
#include "usb.h"
#include <freertos/semphr.h>

static const uint16_t TONEX_ONE_USB_DEVICE_VID = 0x1963;
static const uint16_t TONEX_ONE_USB_DEVICE_PID = 0x00d1;

static const char *TAG = "TONEX_CONTROLLER_TONEX";

void Tonex::handleMessage(std::vector<uint8_t> raw)
{
    auto currentTime = std::chrono::steady_clock::now();

    if (!buffer.empty() && (currentTime - lastByteTime) > messageTimeout)
    {
        // Timeout occurred, process and clear the existing buffer
        processBuffer();
    }

    for (const auto &byte : raw)
    {
        buffer.push_back(byte);
        lastByteTime = currentTime;

        if (byte == 0x7E && buffer.size() > 1 && buffer.front() == 0x7E)
        {
            // Complete message received
            processBuffer();
        }
    }
}

void Tonex::processBuffer()
{
    if (buffer.size() >= 2 && buffer.front() == 0x7E && buffer.back() == 0x7E)
    {
        auto [status, state] = parse(buffer);
        if (status == Status::OK && state.header.type == Type::StateUpdate)
        {
            xSemaphoreTake(semaphore, pdMS_TO_TICKS(1000));
            this->state = static_cast<State>(state);
            xSemaphoreGive(semaphore);
        }
    }
    buffer.clear();
}
// hello?
// 7e
// b903
// 00
// 82 04 00
// 80 0b 01
// b902
//     02
//     0b
// 178c crc
// 7e

void Tonex::init()
{
    semaphore = xSemaphoreCreateBinary();
    usb = USB::init(TONEX_ONE_USB_DEVICE_VID, TONEX_ONE_USB_DEVICE_PID, std::bind(&Tonex::handleMessage, this, std::placeholders::_1));
    requestState();
}

void Tonex::requestState()
{
    std::vector<uint8_t> request = {0xb9, 0x03, 0x00, 0x82, 0x06, 0x00, 0x80, 0x0b, 0x03, 0xb9, 0x02, 0x81, 0x06, 0x03, 0x0b};
    auto framed = hdlc::addFraming(request);
    usb->send(framed);
}

void Tonex::setSlot(Slot newSlot)
{
    uint16_t size = state.raw.size() & 0xFFFF;
    std::cout << "Size: " << (size & 0xFF) << " " << ((size >> 8) & 0xFF) << std::endl;
    std::vector<uint8_t> message = {0xb9, 0x03, 0x81, 0x06, 0x03, 0x82, static_cast<uint8_t>(size & 0xFF), static_cast<uint8_t>((size >> 8) & 0xFF), 0x80, 0x0b, 0x03};
    xSemaphoreTake(semaphore, pdMS_TO_TICKS(1000));
    state.currentSlot = newSlot;
    std::vector<uint8_t> raw(state.raw.begin(), state.raw.end());
    state.raw[state.raw.size() - 5] = static_cast<uint8_t>(newSlot);
    message.insert(message.end(), raw.begin(), raw.end());
    xSemaphoreGive(semaphore);
    usb->send(message);
}

uint16_t Tonex::parseValue(const std::vector<uint8_t> &message, size_t &index)
{
    uint16_t value = 0;
    if (message[index] == 0x81 || message[index] == 0x82)
    {
        value = (message[index + 2] << 8) | message[index + 1];
        index += 3;
    }
    else if (message[index] == 0x80)
    {
        value = message[index + 1];
        index += 2;
    }
    else
    {
        value = message[index];
        index++;
    }
    return value;
}

std::tuple<Status, Message> Tonex::parse(const std::vector<uint8_t> &message)
{
    auto [status, unframed] = hdlc::removeFraming(message);
    if (status != hdlc::Status::OK)
    {
        return {Status::InvalidMessage, {}};
    }
    if (unframed.size() < 7)
    {
        std::cout << "Message too short" << std::endl;
        return {Status::InvalidMessage, {}};
    }
    if (unframed[0] != 0xb9 || unframed[1] != 0x03)
    {
        std::cout << "Invalid header" << std::endl;
        return {Status::InvalidMessage, {}};
    }
    Header header;
    size_t index = 2;
    auto type = parseValue(unframed, index);
    switch (type)
    {
    case 0x0306:
        header.type = Type::StateUpdate;
        break;
    default:
        header.type = Type::Unknown;
        break;
    };
    header.size = parseValue(unframed, index);
    header.unknown = parseValue(unframed, index);
    std::cout << "Structure ID: " << header.type << std::endl;
    std::cout << "Size: " << header.size << std::endl;
    std::cout << "Unknown: " << header.unknown << std::endl;

    if (unframed.size() - index != header.size)
    {
        std::cout << "Invalid message size" << std::endl;
        return {Status::InvalidMessage, {}};
    }

    if (header.type != Type::StateUpdate)
    {
        std::cout << "Not a state structure. Skipping." << std::endl;
        return {Status::OK, {header}};
    }

    State state;
    std::vector<uint8_t> raw(unframed.begin() + index, unframed.end());
    state.raw = raw;
    index += raw.size() - 12;
    state.slotAPreset = unframed[index];
    index += 2;
    state.slotBPreset = unframed[index];
    index += 2;
    state.slotCPreset = unframed[index];
    index += 3;
    state.currentSlot = static_cast<Slot>(unframed[index]);
    ESP_LOGI(TAG, "Current slot: %d", static_cast<int>(state.currentSlot));
    return {Status::OK, state};
}
