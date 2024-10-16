# Tonex ONE USB communication protocol

## Overview
Tonex ONE uses CDC 1.1 (Communications Device Class) protocol for USB communication. It allows to send and receive arbitrary data like in serial communication.

## Disclaimer
This document is not an official documentation. It is a result of reverse engineering and may contain errors. Use it at your own risk.

## Message structure
HDLC (High-Level Data Link Control) protocol is used for message framing. 0x7E is used as a flag, 0x7D is used as an escape character. More information on HDLC protocol can be found [here](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control).

HDLC includes a CRC checksum that needs to be calculated and appended to every message.

## Message format
Each message starts and ends with 0x7E byte. Just before ending byte checksum using CRC 16-CCITT is appended which is calculated using all bytes between 0x7E bytes. Lookup table is used to calculate checksum.

## Message types
There are multiple supported message types. All messages are show without HDLC framing. 

### Hello
This message is sent to Tonex ONE as a first message.

0xb9, 0x03, 0x00, 0x82, 0x04, 0x00, 0x80, 0x0b, 0x01, 0xb9, 0x02, 0x02, 0x0b

0xb9 indicates that there will be multiple elements after this byte. 0x03 indicates that there are 3 elements.
0x00 is a simple 1 byte numeric value. 0x82 indicates that there will be 2 bytes after this byte. 0x04 and 0x00 are 2 bytes numeric value. 

