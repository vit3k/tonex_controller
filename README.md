# TONEX ONE MIDI Adapter

## Overview
The TONEX ONE is a great, little device, but it has one major drawback - it does not support MIDI. This project aims to overcome this limitation by allowing users to change active presets on the TONEX ONE using a USB connection. 

## Project Description
This adapter is based on the ESP32-S3 microcontroller and translates incoming MIDI program change messages to proprietary commands for the TONEX ONE. This enhancement allows the TONEX ONE to be used on complex pedalboards with MIDI controllers, significantly expanding its versatility in live performance and studio settings.

It is built using ESP-IDF (Espressif IoT Development Framework).

## Hardware Required
To run this project you need:
- ESP32-S3 board
- MIDI input circuit connected to PIN 5 of ESP32-S3

The MIDI input circuit can be built using the design available at:
https://www.notesandvolts.com/2015/02/midi-and-arduino-build-midi-input.html

Connection details:
| Target                | Pin |
|-----------------------|-----|
| Receive Data (RxD)    | 5   |
| Ground                | GND |

Note: Basic soldering skills may be required for assembling the MIDI input circuit. Ensure you have the necessary tools and experience before beginning the hardware assembly.

## Configure the project
Set the target to ESP32-S3:
```
idf.py set-target esp32s3
```

## Build and Flash
Build the project and flash it to the board, then run monitor tool to view serial output:
```
idf.py -p PORT flash monitor
```
(To exit the serial monitor, type `Ctrl-]`.)

## Troubleshooting
If you are on Mac and you get an error during flashing, you may need to install a driver. You can download it from: https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html

After installing the driver, use the device `/dev/cu.wchusbserial*` for flashing.

This project bridges the gap between MIDI-capable pedalboards and the TONEX ONE, enabling seamless integration in professional audio setups.