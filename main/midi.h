#pragma once

class ProgramChange
{
public:
    uint8_t channel;
    uint8_t programNumber;
};

void midi_receiver(void *arg);