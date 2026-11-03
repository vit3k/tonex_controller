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

#include "tonex.h"
#include "esp_log.h"
#include "hdlc.h"
#include "usb.h"
#include <freertos/semphr.h>

static const uint16_t TONEX_ONE_USB_DEVICE_VID = 0x1963;
static const uint16_t TONEX_ONE_USB_DEVICE_PID = 0x00d1;

static const char *TAG = "TONEX_CONTROLLER_TONEX";

// State payload tail layout (offsets from the end of raw):
// -18 slot A preset, -16 slot B preset, -14 slot C preset, -11 current slot.
static const size_t STATE_MIN_SIZE = 18;

static const int HANDSHAKE_ATTEMPTS = 3;
static const uint32_t HANDSHAKE_TIMEOUT_MS = 2000;

// Polls until connectionState reaches at least `target`. Returns false on timeout or unplug.
bool Tonex::waitForConnectionState(ConnectionState target, uint32_t timeoutMs)
{
    for (uint32_t waited = 0; waited < timeoutMs; waited += 100)
    {
        if (!usb->isConnected() || connectionState == ConnectionState::Disconnected)
        {
            return false;
        }
        if (connectionState >= target)
        {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return connectionState >= target;
}

void Tonex::onConnection()
{
    ESP_LOGI(TAG, "Connected");
    for (int attempt = 1; attempt <= HANDSHAKE_ATTEMPTS; attempt++)
    {
        if (!usb->isConnected())
        {
            break;
        }
        connectionState = ConnectionState::Connected;
        hello();
        if (!waitForConnectionState(ConnectionState::Helloed, HANDSHAKE_TIMEOUT_MS))
        {
            ESP_LOGW(TAG, "Hello timed out (attempt %d/%d)", attempt, HANDSHAKE_ATTEMPTS);
            continue;
        }
        ESP_LOGI(TAG, "Helloed");
        requestState();
        if (!waitForConnectionState(ConnectionState::StateInitialized, HANDSHAKE_TIMEOUT_MS))
        {
            ESP_LOGW(TAG, "State request timed out (attempt %d/%d)", attempt, HANDSHAKE_ATTEMPTS);
            continue;
        }
        ESP_LOGI(TAG, "Initialized");
        return;
    }
    // Give up; usb_host_task goes back to waiting for the disconnect. A replug retries.
    ESP_LOGE(TAG, "Handshake failed, pedal not initialized");
    if (!usb->isConnected())
    {
        connectionState = ConnectionState::Disconnected;
    }
}

void Tonex::onDisconnection()
{
    ESP_LOGI(TAG, "Disconnected");
    connectionState = ConnectionState::Disconnected;
}

void Tonex::init()
{
    semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(semaphore);
    usb = USB::init(TONEX_ONE_USB_DEVICE_VID, TONEX_ONE_USB_DEVICE_PID, std::bind(&Tonex::handleMessage, this, std::placeholders::_1));
    usb->setConnectionCallback(std::bind(&Tonex::onConnection, this));
    usb->setDisconnectionCallback(std::bind(&Tonex::onDisconnection, this));
}

void Tonex::requestState()
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
    std::vector<uint8_t> request = {0xb9, 0x03, 0x00, 0x82, 0x06, 0x00, 0x80, 0x0b, 0x03, 0xb9, 0x02, 0x81, 0x06, 0x03, 0x0b};
    auto framed = hdlc::addFraming(request);
    usb->send(framed);
    xSemaphoreGive(semaphore);
}

void Tonex::hello()
{
    xSemaphoreTake(semaphore, portMAX_DELAY);
    std::vector<uint8_t> request = {0xb9, 0x03, 0x00, 0x82, 0x04, 0x00, 0x80, 0x0b, 0x01, 0xb9, 0x02, 0x02, 0x0b};
    auto framed = hdlc::addFraming(request);
    usb->send(framed);
    xSemaphoreGive(semaphore);
}

void Tonex::setSlot(Slot newSlot)
{
    if (connectionState != ConnectionState::StateInitialized) 
    {
        ESP_LOGW(TAG, "Tonex connection is not ready");
        return;
    }
    ESP_LOGI(TAG, "Setting slot %d", static_cast<int>(newSlot));
    xSemaphoreTake(semaphore, portMAX_DELAY);
    if (state.raw.size() < STATE_MIN_SIZE)
    {
        xSemaphoreGive(semaphore);
        ESP_LOGW(TAG, "State too short to set slot: %u", static_cast<unsigned>(state.raw.size()));
        return;
    }
    uint16_t size = state.raw.size() & 0xFFFF;
    std::vector<uint8_t> message = {0xb9, 0x03, 0x81, 0x06, 0x03, 0x82, static_cast<uint8_t>(size & 0xFF), static_cast<uint8_t>((size >> 8) & 0xFF), 0x80, 0x0b, 0x03};
    state.currentSlot = newSlot;
    state.raw[state.raw.size() - 11] = static_cast<uint8_t>(newSlot);
    message.insert(message.end(), state.raw.begin(), state.raw.end());
    auto framed = hdlc::addFraming(message);
    xSemaphoreGive(semaphore);
    usb->send(framed);
}

void Tonex::changePreset(Slot slot, uint8_t preset)
{
    // TODO: update to 1.2.* needed
    if (connectionState != ConnectionState::StateInitialized) 
    {
        ESP_LOGW(TAG, "Tonex connection is not ready");
        return;
    }
    if (preset >= 20)
    {
        ESP_LOGW(TAG, "Invalid preset number: %d", preset);
        return;
    }
    ESP_LOGI(TAG, "Changing preset for slot %d to %d", static_cast<int>(slot), preset);
    xSemaphoreTake(semaphore, portMAX_DELAY);
    if (state.raw.size() < STATE_MIN_SIZE)
    {
        xSemaphoreGive(semaphore);
        ESP_LOGW(TAG, "State too short to change preset: %u", static_cast<unsigned>(state.raw.size()));
        return;
    }
    uint16_t size = state.raw.size() & 0xFFFF;
    std::vector<uint8_t> message = {0xb9, 0x03, 0x81, 0x06, 0x03, 0x82, static_cast<uint8_t>(size & 0xFF), static_cast<uint8_t>((size >> 8) & 0xFF), 0x80, 0x0b, 0x03};
    switch (slot)
    {
    case Slot::A:
        state.slotAPreset = preset;
        state.raw[state.raw.size() - 18] = preset;
        break;
    case Slot::B:
        state.slotBPreset = preset;
        state.raw[state.raw.size() - 16] = preset;
        break;
    case Slot::C:
        state.slotCPreset = preset;
        state.raw[state.raw.size() - 14] = preset;
        break;
    }
    message.insert(message.end(), state.raw.begin(), state.raw.end());
    auto framed = hdlc::addFraming(message);
    xSemaphoreGive(semaphore);
    usb->send(framed);
}

Slot Tonex::getCurrentSlot()
{
    return state.currentSlot;
}

uint8_t Tonex::getPreset(Slot slot)
{
    switch(slot)
    {
        case Slot::A:
            return state.slotAPreset;
        case Slot::B:
            return state.slotBPreset;
        case Slot::C:
            return state.slotCPreset;
    }

    return 0;
}

void Tonex::switchSilently(uint8_t value)
{
    auto notActiveSlot = state.currentSlot == Slot::A ? Slot::B : Slot::A;
    changePreset(notActiveSlot, value);
    setSlot(notActiveSlot);
}

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
        auto [status, msg] = parse(buffer);
        if (status != Status::OK)
        {
            buffer.clear();
            ESP_LOGE(TAG, "Error parsing message: %d", static_cast<int>(status));
            return;
        }

        //ESP_LOG_BUFFER_HEX(TAG, buffer.data(), buffer.size());

        switch (msg->header.type)
        {
        case Type::StateUpdate:
            xSemaphoreTake(semaphore, portMAX_DELAY);
            {
                this->state = *static_cast<State *>(msg.get());
                ESP_LOGI(TAG, "Received StateUpdate. Current slot: %d", static_cast<int>(this->state.currentSlot));
                connectionState = ConnectionState::StateInitialized;
            }
            xSemaphoreGive(semaphore);
            break;
        case Type::Hello:
            ESP_LOGI(TAG, "Received Hello");
            xSemaphoreTake(semaphore, portMAX_DELAY);
            connectionState = ConnectionState::Helloed;
            xSemaphoreGive(semaphore);
            break;
        default:
            ESP_LOGI(TAG, "Message unknown");
            break;
        }
    }
    buffer.clear();
}

bool Tonex::parseValue(const std::vector<uint8_t> &message, size_t &index, uint16_t &value)
{
    if (index >= message.size())
    {
        ESP_LOGW(TAG, "parseValue: index %u out of range (size %u)", static_cast<unsigned>(index), static_cast<unsigned>(message.size()));
        return false;
    }
    size_t needed = (message[index] == 0x81 || message[index] == 0x82) ? 3 : (message[index] == 0x80 ? 2 : 1);
    if (message.size() - index < needed)
    {
        ESP_LOGW(TAG, "parseValue: truncated value at index %u (size %u)", static_cast<unsigned>(index), static_cast<unsigned>(message.size()));
        return false;
    }
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
    return true;
}

std::tuple<Status, std::unique_ptr<Message>> Tonex::parse(const std::vector<uint8_t> &message)
{
    auto [status, unframed] = hdlc::removeFraming(message);
    if (status != hdlc::Status::OK)
    {
        return {Status::InvalidMessage, nullptr};
    }
    if (unframed.size() < 5)
    {
        ESP_LOGE(TAG, "Message too short");
        return {Status::InvalidMessage, nullptr};
    }
    if (unframed[0] != 0xb9 || unframed[1] != 0x03)
    {
        ESP_LOGE(TAG, "Invalid header");
        return {Status::InvalidMessage, nullptr};
    }
    Header header;
    size_t index = 2;
    uint16_t type;
    if (!parseValue(unframed, index, type))
    {
        return {Status::InvalidMessage, nullptr};
    }
    switch (type)
    {
    case 0x0306:
        header.type = Type::StateUpdate;
        break;
    case 0x02:
        header.type = Type::Hello;
        break;
    default:
        header.type = Type::Unknown;
        break;
    };
    if (!parseValue(unframed, index, header.size) || !parseValue(unframed, index, header.unknown))
    {
        return {Status::InvalidMessage, nullptr};
    }
    ESP_LOGI(TAG, "Structure ID: %d", header.type);
    ESP_LOGI(TAG, "Size: %d", header.size);

    if (unframed.size() - index != header.size)
    {
        ESP_LOGE(TAG, "Invalid message size");
        return {Status::InvalidMessage, nullptr};
    }

    switch (header.type)
    {
    case Type::Hello:
    {
        auto msg = std::make_unique<Message>();
        msg->header = header;
        return {Status::OK, std::move(msg)};
    }
    case Type::StateUpdate:
        return parseState(unframed, index);
    default:
    {
        ESP_LOGI(TAG, "Unknown structure. Skipping.");
        auto msg = std::make_unique<Message>();
        msg->header = header;
        return {Status::OK, std::move(msg)};
    }
    };
}

std::tuple<Status, std::unique_ptr<State>> Tonex::parseState(const std::vector<uint8_t> &unframed, size_t &index)
{
    static const char slotName[] ={'A', 'B', 'C'};
    if (index > unframed.size() || unframed.size() - index < STATE_MIN_SIZE)
    {
        ESP_LOGW(TAG, "State message too short");
        return {Status::InvalidMessage, nullptr};
    }
    auto state = std::make_unique<State>();
    state->header.type = Type::StateUpdate;
    std::vector<uint8_t> raw(unframed.begin() + index, unframed.end());
    state->raw = raw;
    index += raw.size() - 18;
    state->slotAPreset = unframed[index];
    index += 2;
    state->slotBPreset = unframed[index];
    index += 2;
    state->slotCPreset = unframed[index];
    index += 3;
    if (unframed[index] > Slot::C)
    {
        ESP_LOGW(TAG, "Invalid current slot: %d", unframed[index]);
        return {Status::InvalidMessage, nullptr};
    }
    state->currentSlot = static_cast<Slot>(unframed[index]);
    ESP_LOGI(TAG, "Current slot: %c", slotName[static_cast<int>(state->currentSlot)]);
    ESP_LOGI(TAG, "Presets: A: %d, B: %d, C: %d", state->slotAPreset, state->slotBPreset, state->slotCPreset);
    initialized = true;
    return {Status::OK, std::move(state)};
}
