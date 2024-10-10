# Tonex One controller

## Hardware Required

To run this project ESP32-S3 is required.

## Setup the Hardware

Connect the external serial interface to the board as follows.

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