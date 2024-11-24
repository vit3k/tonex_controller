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

#pragma once 
#include <cstdint>
#include <vector>
#include <tuple>
#include "usb.h"
#include <freertos/semphr.h>
#include <chrono>

enum Status {
    OK,
    InvalidMessage,

};
enum Type {
    Unknown,
    StateUpdate,
    Hello
};
enum Slot
{
    A = 0,
    B = 1,
    C = 2
};
struct Header
{
    Type type;
    uint16_t size;
    uint16_t unknown;
};
struct Message {
    Header header;
};
struct State : public Message
{
    uint8_t slotAPreset;
    uint8_t slotBPreset;
    uint8_t slotCPreset;
    Slot currentSlot;
    std::vector<uint8_t> raw;
};

enum ConnectionState {
    Disconnected,
    Connected,
    Helloed,
    StateInitialized
};
class Tonex
{
private:
    ConnectionState connectionState = ConnectionState::Disconnected;
    SemaphoreHandle_t semaphore;
    std::unique_ptr<USB> usb; 
    State state;
    uint16_t parseValue(const std::vector<uint8_t> &message, size_t &index);
    std::tuple<Status, Message*> parse(const std::vector<uint8_t> &message);
    std::tuple<Status, State*> parseState(const std::vector<uint8_t> &unframed, size_t &index);
    std::vector<uint8_t> buffer;
    std::chrono::steady_clock::time_point lastByteTime;
    const std::chrono::milliseconds messageTimeout{1000}; 
    void processBuffer();
    bool initialized;
    void onConnection();
    void requestState();
    void hello();
    
public:
    void setSlot(Slot slot);
    void handleMessage(std::vector<uint8_t> raw);
    void init();
    void changePreset(Slot slot, uint8_t value);
    Slot getCurrentSlot();
    uint8_t getPreset(Slot slot);
    void switchSilently(uint8_t value);
};