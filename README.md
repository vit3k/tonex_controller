# TONEX ONE MIDI Adapter

![License](https://img.shields.io/badge/License-GPLv3-blue.svg)
![Issues](https://img.shields.io/github/issues/yourusername/tonex-one-midi-adapter)
![Stars](https://img.shields.io/github/stars/yourusername/tonex-one-midi-adapter)

## Table of Contents
- [Overview](#overview)
- [Disclaimer](#disclaimer)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Installation](#installation)
- [Configuration](#configuration)
- [Usage](#usage)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Overview

The TONEX ONE MIDI Adapter is a project that enables MIDI support for the TONEX ONE device. It allows users to change active presets on the TONEX ONE using a USB connection, significantly enhancing its versatility in live performance and studio settings.

## Disclaimer

**IMPORTANT: Please read this disclaimer carefully before using the TONEX ONE MIDI Adapter.**

This project is an unofficial modification for the TONEX ONE device and is not affiliated with, endorsed by, or supported by the manufacturer of TONEX ONE. By using this adapter, you acknowledge and agree to the following:

1. **Use at Your Own Risk**: This adapter involves interfacing with the TONEX ONE device in ways not officially supported by the manufacturer. Using this adapter may void your warranty, potentially damage your device, or lead to unexpected behavior.

2. **No Warranty**: This project is provided "as is", without warranty of any kind, express or implied. The creators and contributors of this project cannot be held responsible for any damage to your equipment or any other consequences resulting from the use of this adapter.

3. **Potential Legal Implications**: The use of this adapter may violate the terms of service or end-user license agreement of your TONEX ONE device. It may also infringe on intellectual property rights. It is your responsibility to ensure that your use of this adapter complies with all applicable laws and agreements.

4. **No Support from Manufacturer**: The manufacturer of TONEX ONE is not obligated to provide support for any issues arising from the use of this unofficial adapter. Using this adapter may affect your ability to receive official support for your device.

5. **Modification and Distribution**: While this project is open-source and you are free to modify and distribute it under the terms of the GPL v3 license, please be aware that doing so may carry its own legal risks.

By proceeding to use this adapter, you indicate that you have read, understood, and agreed to this disclaimer. If you do not agree with these terms, do not use this adapter.

It is recommended to consult with a legal professional if you have any concerns about the use or distribution of this project.

## Features

- Translates MIDI Program Change messages to TONEX ONE commands
- Based on the ESP32-S3 microcontroller
- Built using ESP-IDF (Espressif IoT Development Framework)
- Enables integration of TONEX ONE into complex pedalboards with MIDI controllers

## Hardware Requirements

To implement this project, you'll need:

1. ESP32-S3 board
2. MIDI input circuit connected to PIN 5 of ESP32-S3

### MIDI Input Circuit

The MIDI input circuit can be constructed using the design available at:
[Notes and Volts - MIDI and Arduino: Build a MIDI Input Circuit](https://www.notesandvolts.com/2015/02/midi-and-arduino-build-midi-input.html)

#### Connection Details

| Target | Pin |
|--------|-----|
| Receive Data (RxD) | 5 |
| Ground | GND |

**Note:** Basic soldering skills may be required for assembling the MIDI input circuit.

## Installation

1. Clone this repository:
   ```
   git clone https://github.com/yourusername/tonex-one-midi-adapter.git
   ```
2. Navigate to the project directory:
   ```
   cd tonex-one-midi-adapter
   ```
3. Set up ESP-IDF environment (if not already done)

## Configuration

1. Set the target to ESP32-S3:
   ```
   idf.py set-target esp32s3
   ```

2. Modify FIFO size configuration:
   - Open file: `components/hal/usb_dwc_hal.c`
   - Locate the `usb_dwc_hal_set_fifo_bias` function
   - Modify as follows:
     ```c
     case USB_HAL_FIFO_BIAS_DEFAULT:
         fifo_config.nptx_fifo_lines = OTG_DFIFO_DEPTH / 4;
     ```

## Usage

The adapter supports MIDI Program Change messages to control the active slot on your TONEX ONE device:

1. **MIDI Channel**: Hardcoded in `midi.cpp` file (`MIDI_CHANNEL` constant)
2. **Program Change Mapping**:
   - Program 1: Changes active slot to A
   - Program 2: Changes active slot to B

To use:
1. Ensure your MIDI controller is connected to the MIDI input circuit
2. Send Program Change messages from your MIDI controller
3. The adapter will translate these to commands for TONEX ONE

## Build and Flash

Build the project and flash it to the board:

```
idf.py -p PORT flash monitor
```

(Exit serial monitor with `Ctrl-]`)

## Troubleshooting

### Flashing Error on Mac

If you encounter a flashing error on Mac:

1. Install the driver from: [CH34XSER_MAC_ZIP](https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html)
2. Use the device `/dev/cu.wchusbserial*` for flashing

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE). See the [LICENSE](LICENSE) file for details.