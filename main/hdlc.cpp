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

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "hdlc.h"

namespace hdlc {

uint16_t calculateCRC(const std::vector<uint8_t> &data) {
    uint16_t crc = 0xFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0x8408;  // 0x8408 is the reversed polynomial x^16 + x^12 + x^5 + 1
            } else {
                crc = crc >> 1;
            }
        }
    }
    return ~crc;
}

void addByteWithStuffing(std::vector<uint8_t> &output, uint8_t byte) {
  if (byte == 0x7E || byte == 0x7D) {
    output.push_back(0x7D);
    output.push_back(byte ^ 0x20);
  } else {
    output.push_back(byte);
  }
}

std::vector<uint8_t> addFraming(const std::vector<uint8_t> &input) {
  std::vector<uint8_t> output;
  output.reserve(input.size() * 2 + 4);
  output.push_back(0x7E); // Start flag

  for (uint8_t byte : input) {
    addByteWithStuffing(output, byte);
  }

  uint16_t crc = calculateCRC(input);
  addByteWithStuffing(output, crc & 0xFF);
  addByteWithStuffing(output, crc >> 8);

  output.push_back(0x7E); // End flag
  return output;
}

std::tuple<Status, std::vector<uint8_t>> removeFraming(const std::vector<uint8_t> &input) {
  if (input.size() < 4 || input.front() != 0x7E || input.back() != 0x7E) {
    return {Status::InvalidFrame, {}};
  }

  std::vector<uint8_t> output;
  for (size_t i = 1; i < input.size() - 1; ++i) {
    if (input[i] == 0x7D) {
      if (i + 1 >= input.size() - 1) {
        return {Status::InvalidEscapeSequence, {}};
      }
      output.push_back(input[i + 1] ^ 0x20);
      ++i;
    } else if (input[i] == 0x7E) {
      break;
    } else {
      output.push_back(input[i]);
    }
  }

  if (output.size() < 2) {
    return {Status::InvalidFrame, {}};
  }

  uint16_t received_crc = (output.back() << 8) | output[output.size() - 2];
  output.resize(output.size() - 2);
  uint16_t calculated_crc = calculateCRC(output);

  if (received_crc != calculated_crc) {
    return {Status::CRCMismatch, {}};
  }

  return {Status::OK, output};
}
};