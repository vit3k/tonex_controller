# Tonex ONE USB Communication Protocol

> This protocol documentation is a work in progress and may be updated as more information becomes available.

## Disclaimer

This document is not official documentation. It is the result of reverse engineering and may contain errors. It is incomplete and contains many unknowns. Use at your own risk.

## Overview

Tonex ONE uses the CDC 1.1 (Communications Device Class) protocol for USB communication. This allows for sending and receiving arbitrary data, similar to serial communication. Communication is structured as messages. 

When something is updated on Tonex ONE pedal it sends a message with this change. App can also send messages that changes something on a pedal or it can ask for a data i.e. state data or preset data.

 

## Message Framing

The HDLC (High-Level Data Link Control) protocol is used for message framing.

- Flag byte: 0x7E
- Escape character: 0x7D

For more information on the HDLC protocol, refer to the [Wikipedia page](https://en.wikipedia.org/wiki/High-Level_Data_Link_Control).

### CRC Checksum

HDLC includes a CRC checksum that must be calculated and appended to every message. The CRC is calculated using all bytes between the 0x7E flag bytes.

**Important note:** 0x7D escaping includes the CRC.

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
0xBA 0x14
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

### Request State

1. Following the hello exchange, the controller sends a *request state* message to the pedal.
2. This message asks for the current state of the pedal.
3. The pedal responds with a *state changed* message.
4. Usually, the pedal also sends preset data in a subsequent message, though this is not always the case.

### State Changed

- Whenever the state of the pedal changes, it sends a *state changed* message to the app.
- This message includes the entire state of the pedal at once.

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

## Considerations for Implementation

When implementing communication with the Tonex ONE pedal:

1. Always start with the hello message exchange to establish communication.
2. Use the request state message to get the current full state before making changes.
3. When changing a specific parameter (like the active slot), modify only that part of the state data.
4. Send the full state back to the pedal, even if only one parameter has changed.
5. Be prepared to handle state changed messages from the pedal at any time, as the state can change due to physical interaction with the pedal.


