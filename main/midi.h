#pragma once

#include <cstdint>

class Tonex;
namespace midi {
    class ProgramChange
    {
    public:
        uint8_t channel;
        uint8_t programNumber;
    };

    void midi_receiver(void *arg);
    void init(Tonex* tonex);
}