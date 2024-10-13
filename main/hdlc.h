#pragma once

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace hdlc {

enum Status {
    OK,
    InvalidFrame,
    InvalidEscapeSequence,
    CRCMismatch
};

std::vector<uint8_t> addFraming(const std::vector<uint8_t> &input);

std::tuple<Status, std::vector<uint8_t>> removeFraming(const std::vector<uint8_t> &input);

};