#include "tonex.h"
#include <iostream>
#include "hdlc.h"

void Tonex::handleMessage(std::vector<uint8_t> raw) {
    auto [status, state] = parse(raw);
    if (status == Status::OK && state.header.type == Type::StateChanged) {
        this->state = static_cast<State>(state);
    }
}

void Tonex::setSlot(Slot newSlot)
{
    state.currentSlot = newSlot;
    std::vector<uint8_t> raw(state.raw.begin(), state.raw.end());
    uint16_t size = raw.size() & 0xFFFF;
    std::cout << "Size: " << (size & 0xFF) << " " << ((size >> 8) & 0xFF) << std::endl;
    std::vector<uint8_t> message = {0xb9, 0x03, 0x81, 0x06, 0x03, 0x82, static_cast<uint8_t>(size & 0xFF), static_cast<uint8_t>((size >> 8) & 0xFF), 0x80, 0x0b, 0x03};

    state.raw[state.raw.size() - 5] = static_cast<uint8_t>(newSlot);
    message.insert(message.end(), raw.begin(), raw.end());
    // Send message to tonex
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
    if (status != hdlc::Status::OK) {
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
    switch(type) {
        case 0x0306:
            header.type = Type::StateChanged;
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

    if (header.type != Type::StateChanged)
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
    std::cout << "Slot A Preset: " << static_cast<int>(state.slotAPreset) << std::endl;
    std::cout << "Slot B Preset: " << static_cast<int>(state.slotBPreset) << std::endl;
    std::cout << "Slot C Preset: " << static_cast<int>(state.slotCPreset) << std::endl;
    std::cout << "Current Slot: " << static_cast<int>(state.currentSlot) << std::endl;
    return {Status::OK, state};
}
