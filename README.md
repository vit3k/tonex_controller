# Tonex One controller

This project allows user to control active presets on TONEX ONE using a USB connection. It is based on ESP32-S3 and translates incoming MIDI program change messages to proprietary commands for the TONEX ONE.

This allows to use TONEX ONE on complex pedalboards with MIDI controllers.

## Hardware Required

To run this project you need:
- ESP32-S3 board
- MIDI input circuit that is connected to PIN 5 of ESP32-S3. I used the one explained here: https://www.notesandvolts.com/2015/02/midi-and-arduino-build-midi-input.html

```
  --------------------------------------------
  | Target chip Interface | Pin              |
  | ----------------------|-------------------
  | Receive Data (RxD)    | 5                |    
  | Ground                | GND              | 
  --------------------------------------------
```
## Configure the project

```
idf.py set-target esp32s3
```

## Build and Flash

Build the project and flash it to the board, then run monitor tool to view serial output:

```
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

## Troubleshooting
If you are on Mac and you get an error during flashing you may try install driver from here: https://www.wch-ic.com/downloads/CH34XSER_MAC_ZIP.html