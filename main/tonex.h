#pragma once 
#include <cstdint>
#include <vector>
#include <tuple>

enum Status {
    OK,
    InvalidMessage,

};
enum Type {
    Unknown,
    StateChanged
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


class Tonex
{
private:
    State state;
    uint16_t parseValue(const std::vector<uint8_t> &message, size_t &index);
    std::tuple<Status, Message> parse(const std::vector<uint8_t> &message);
public:
    void setSlot(Slot slot);
    void handleMessage(std::vector<uint8_t> raw);
};