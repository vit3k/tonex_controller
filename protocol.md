# Tonex ONE USB Communication Protocol

> This protocol documentation is a work in progress and may be updated as more information becomes available.

## Disclaimer

This document is not official documentation. It is the result of reverse engineering and may contain errors. It is incomplete and contains many unknowns. Use at your own risk.

## Overview

Tonex ONE uses the CDC 1.1 (Communications Device Class) protocol for USB communication. This allows for sending and receiving arbitrary data, similar to serial communication. Communication is structured as messages. 

When something is updated on Tonex ONE pedal it sends a message with this change. App can also send messages that changes something on a pedal or it can ask for a data i.e. state data or preset data.

## Message Framing

The HDLC (High-Level Data Link Control) protocol is used for message framing. It looks like only asynchronous framing part is used, without addressing and control fields.

- Flag byte: 0x7E
- Byte stuffing is used 

For more information on the HDLC protocol, refer to the [Wikipedia page](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control).

### CRC Checksum

HDLC includes a CRC-CCITT checksum that must be calculated and appended to every message. The CRC is calculated using all bytes between the 0x7E flag bytes.

**Important note:** byte stuffing is also applied to CRC.

### Message Structure

| Start | Message | CRC | End |
|-------|---------|-----|-----|
| 0x7E  | Payload | 2 bytes | 0x7E |

Example:
```
0x7E 0x01 0x02 0x03 0x12 0x34 0x7E
```

## Message Format
Messages have an 'object'-like structure with defined TAGs for various data types.

### List / Object Types
There are several 'list' / 'object' types, though the differences between them are unclear:

- 0xB9
- 0xBA
- 0xBC

Each of these tags is followed by the number of items in the collection.

Example:
```
0xB9 0x03
```
This indicates a collection with 3 elements.

Elements can be:
- 1-byte number
- Another tag (e.g., 0x80, 0x81, 0x82)
- Another collection

Examples:
```
0xB9 0x03 0x01 0x02 0x03

0xBA 0x14 (list with 20 elements)
0xB9 0x03 0x00 0x80 0xFF 0x00
0xB9 0x03 0x80 0xFF 0x3F 0x00
. . . (18 more elements)
```

**Note:** It's unclear why 0xFF is 'escaped' using a 0x80 prefix in some cases, or how this relates to the 0x80, 0x81, 0x82 prefixes.

### Number Types
#### 1-byte Number
A simple 1-byte number. Example: `0x02`

#### 2-byte Numbers
Prefixed with either 0x80, 0x81, or 0x82. Uses little-endian encoding.

Examples:
```
0x80 0x02 0x00 (2)
0x81 0xD1 0x01 (465)
0x82 0x02 0x00 (2)
```

#### Float Numbers
Prefixed with 0x88, followed by 4 bytes of float data.

Example:
```
0x88 0x00 0x00 0x58 0xC1 (0)
```

## Message Structure
Each message consists of a header and a body.

### Header
The header typically follows this structure:
```
0xB9 0x03
0x81 [2 bytes - message type]
0x82 [2 bytes - message size (excluding header)]
0x80 [2 bytes - unknown purpose]
```

Example (request state message header):
```
0xB9 0x03
0x81 0x06 0x03
0x82 0x95 0x00
0x80 0x0B 0x03
```

Example (hello message header):
```
0xB9 0x03
0x00
0x82 0x04 0x00
0x80 0x0B 0x01
```

### Body
The body is usually a list of elements. Its length is specified in the header.

Example (hello message body):
```
0xB9 0x02
0x02
0x0B
```

## Message Flow
The communication between the Tonex controller and the pedal follows a specific sequence of messages. Understanding this flow is crucial for implementing proper communication with the device.

### Hello
1. The Tonex controller initiates communication by sending a *hello* message to the pedal.
2. The pedal responds with various data, including its firmware version.

Example hello message:
```
7e
b9 03
    00
    82 04 00
    80 0b 01
b902 
    02
    0b 
17 8c crc
7e
```

Example response:
```
7e
b9 03
    02
    2b
    0b
b9 07
    00
    80 c7 
    b9 03 02 00 00 
    b9 03 01 01 03 [firmware version - 1.1.3]
    bc 14 d7 4b e1 30 01 bf 7a 0d 2b 2e 7a a0 22 81 e0 c7 75 f0 0a 5e 
    82 a9 
    9a 04 00 00 
    
d8 d6 
7e
```
### Request State

1. Following the hello exchange, the controller sends a *request state* message to the pedal.
2. This message asks for the current state of the pedal.
3. The pedal responds with a *state changed* message.
4. Usually, the pedal also sends preset data in a subsequent message, though this is not always the case.

Example:
```
7e

b9 03 
    00
    82 06 00
    80 0b 03
b9 02 
    81 06 03 
    0b

44 66
7e
```
### State Changed

- Whenever the state of the pedal changes, it sends a *state changed* message to the app.
- This message includes the entire state of the pedal at once.

Example of state changed message (without framing):
```
b9 03 
    81 06 03 
    80 97 
    02 
b9 01 
    b9 0b 
        88 00 00 70 41 [ inputTrim // float32   0x000070c1 (-15.0) -> 0x000058c1 (0) -> 0x00007041 (15.0) ]
        88 33 33 0b 41 
        00 
        00 [ cabsimBypass 0x00 - off, 0x01 - on ]
        01 [ tunningMode 0x00 - mute, 0x01 - thru]
        ba 14 [ array of colors for each preset ]
            b9 03 
                00 
                80 ff 
                00 
            b9 03 11 00 00 [ R G B]
            b9 03 80 ff 3f 00 [ for some reason 0xFF value is escaped with 0x80 in colors]
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 80 ff 00 00 
            b9 03 11 00 00 
        bc 06 
            00 [preset number in slot A]
            00 
            02 [preset number in slot B]
            00 
            05 [preset number in slot C]
            00 
        00 
        00 [active slot 0 - A, 1 - B, 3 - C]
        81 d1 01 [ a4Reference 0x019f (415 hz) -> 0x01b8 (440 hz) -> 0x01d1 (465 hz) ]
        00 [ direct monitoring 0x00 - off, 0x01 - on ]
        [ added in 1.2 firmware version ]
        00 [ tempo source 00 - GLOBAL, 01 - PRESET]
        88 00 00 70 42 [ tempo in BPM // float32 0x00007042 (60) -> 0x00007043 (240) ]
        
```
### Changing the State

The Tonex controller uses *set state* messages to modify the pedal's state, such as changing the current active slot. However, this process has some important considerations:

1. The *set state* message includes all aspects of the pedal's state, including:
   - Preset assignments to slots
   - Active slot
   - Direct monitoring status
   - A4 reference frequency
   - Colors of presets

2. Due to this comprehensive state update, the controller must be careful when making changes:
   - It should first read the current state of the pedal.
   - Then, it should modify only the needed data (e.g., active slot) within the full state.
   - Finally, it sends the modified full state back to the pedal.

This approach ensures that the controller doesn't inadvertently change other aspects of the pedal's state when updating a specific parameter.

Example:
```
7e 
b903 
81 06 03 
82 98 00 [ message size 0x0098 (152) ]
80 0b 03 
... the rest of message is like state changed one
9f8b crc
7e
```

### Request preset
It is possible to request preset information.

Example:
```
7e

b9 03
    81 00 03 
    82 06 00
    80 0b 03 
b9 04 
    0b 
    01
    00
    00 [ presetNumber ]

c827
7e
```

Tonex ONE responds with a long message that was not yet analized (it is a message based on 1.1.4 firmwawe. Probably needs updating to match 1.2.* version with FX). 
```
b9 03 
    81 04 03 
    81 9c 04 
    02 
    b9 03 
        00 
        00 
        b9 04 
            b9 02 
                bc 21 42 61 7a 50 6c 65 78 69 54 72 65 62 6c 65 43 72 75 6e 63 68 00 00 00 00 00 00 00 00 00 00 00 00 00 14 [ preset name ]
                ba 01 
                    88 00 00 00 00 
            ba 03 
                ba 29 
                    88 00 00 00 00 
                    88 00 00 c8 c2 
                    88 00 00 a0 41 
                    88 00 00 70 c2 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 00 c1 
                    88 00 00 a0 40 
                    88 00 00 80 3f 
                    88 00 00 a0 40 
                    88 00 00 96 43 
                    88 00 00 a0 40 
                    88 33 33 33 3f 
                    88 00 80 3b 44 
                    88 00 00 a0 40 
                    88 00 00 fa 44 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 c8 42 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 c8 41 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 80 3f 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 80 40 
                    88 00 00 a0 40 
                    88 00 00 70 42 
                    88 00 00 00 00 
                    88 00 00 20 41 
                ba 29 
                    88 00 00 00 00 
                    88 00 00 c8 c2 
                    88 00 00 a0 41 
                    88 00 00 70 c2 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 80 3f 
                    88 00 00 a0 40 
                    88 00 00 96 43 
                    88 00 00 a0 40 
                    88 33 33 33 3f 
                    88 00 80 3b 44 
                    88 00 00 a0 40 
                    88 00 80 ed 44 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 c8 42 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 c8 41 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 80 3f 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 40 40 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 00 00 
                ba 29 
                    88 00 00 00 00 
                    88 00 00 c8 c2 
                    88 00 00 a0 41 
                    88 00 00 70 c2 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 80 3f 
                    88 00 00 a0 40 
                    88 00 00 96 43 
                    88 00 00 a0 40 
                    88 33 33 33 3f 
                    88 00 80 3b 44 
                    88 00 00 a0 40 
                    88 00 80 ed 44 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 c8 42 
                    88 00 00 80 3f 
                    88 00 00 00 00 
                    88 00 00 c8 41 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 80 3f 
                    88 00 00 c0 3f 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 a0 40 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 40 40 
                    88 00 00 a0 40 
                    88 00 00 00 00 
                    88 00 00 00 00 
                    88 00 00 00 00 
        b9 0d 
            b9 02 
                bc 21 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
                00 
            b9 02 
                bc 0b 32 30 32 34 2d 30 37 2d 33 30 00 
                0a 
            b9 02 
                bc 21 44 52 49 56 45 00 69 20 50 61 63 6b 20 28 62 61 7a 6f 6b 2e 65 75 29 00 00 00 00 00 00 00 00 00 00 
                05 
            b9 02 
                bc 21 45 6c 6563 74 72 69 63 20 47 75 69 74 61 72 00 00 00 00 44 52 49 56 45 00 00 00 00 00 00 00 00 00 
                0f 
            b9 02 
                bc 21 53 6f 6c 69 64 20 42 6f 64 79 00 00 4d 61 72 73 68 61 6c 6c 20 53 56 32 30 48 00 00 00 00 00 00 00 
                0a 
            b9 02 
                bc 21 42 72 69 64 67 65 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
                06 
            b9 02 
                bc 21 53 2d 53 2d 48 20 28 46 61 74 20 53 74 72 2e 29 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 c0 
                10 
            b9 02 
                bc 21 42 61 7a 6f 6b 00 00 00 60 fc 26 6b 01 00 00 00 09 00 00 00 00 00 00 00 00 ec a5 21 01 00 00 00 50 
                05 
            b9 02 
                bc 21 00 ef 6d 0b 00 60 00 00 50 ef 6d 0b 00 60 00 00 12 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00 40 
                00 
            b9 02 
                bc 21 00 fc 26 6b 01 00 00 00 20 fc 26 6b 01 00 00 00 b8 67 16 05 01 00 33 18 04 00 00 00 00 00 00 00 e0 
                00 
            b9 02 
                bc 21 00 09 08 18 01 00 00 00 18 6f 27 6b 01 00 00 00 f6 ff ff ff 00 00 00 00 14 00 00 00 00 00 00 00 a8 
                00 
            b9 02 
                bc 21 00 40 a8 0a 01 00 00 00 08 2f a9 09 01 00 00 00 00 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
                00 
            b9 02 
                bc 41 46 72 6f 6d 20 42 61 7a 50 6c 65 78 69 20 50 61 63 6b 20 28 79 6f 75 20 63 61 6e 20 66 69 6e 64 20 69 74 20 6f 6e 20 68 74 74 70 73 3a 2f 2f 62 61 7a 6f 6b 2e65 75 29 00 ff ff ff 00 00 00 00 00 
                38

```
## Considerations for Implementation

When implementing communication with the Tonex ONE pedal:

1. Although app sends hello message as first it looks like it is not necessary. But because of my lack of full understanding of the protocol I choosed to send it in Tonex controller anyway.
2. Use the request state message to get the current full state before making changes.
3. When changing a specific parameter (like the active slot), modify only that part of the state data.
4. Send the full state back to the pedal, even if only one parameter has changed.
5. Be prepared to handle state changed messages from the pedal at any time, as the state can change due to physical interaction with the pedal.
