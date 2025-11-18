# pixelflut server

## Building (SDL3 canvas)
- install SDL3 development files (fedora: `sudo dnf install SDL3-devel`)
- run `make`. Use the makefile to change build directory (default is `./build`)

## Protocol

### General Remarks

The protocol is versioned (see Info command). Later versions may add more commands.

This server implements a binary protocol. Integers are sent in **little-endian** format.

A range specified as `start..end` is exclusive, so the range `1..3` contains bytes 1 and 2.

#### Coordinates outside screen bounds

It is allowed to specify coordinates outside screen bounds. In case of reading, pixels outside the screen are interpreted as black (r = g = b = 0). In case of writing, pixels outside the screen are ignored.

#### Ignored bytes

Some commands are smaller than 8 bytes, and the remaining memory is ignored. Nevertheless, all 8 bytes must be sent to the server before the command is processed.

#### Send and receive order

The server may stop processing further commands from a client if its send buffer is full. The send buffer size can be discovered with the INFO command. For example, for a send buffer size of 1024 the server holds a maximum of 1024 / 4 = 256 color values. This means at most 256 GET commands may be sent to the server before the client must read from the server. Of course, this is a conservative guarantee, as more bytes are likely in-flight. However, a client not adhering to this is considered erroneous.

### Info

| Byte | Content      |
| ----:| ------------ |
| 0    | `'I' (0x49)` |
| 1..8 | ignored      |

#### Response format

| Byte   | Content              |
| ------:| -------------------- |
| 0..4   | protocol version (1) |
| 4..8   | screen width         |
| 8..12  | screen height        |
| 12..16 | receive buffer size  |
| 16..20 | send buffer size     |



### Print pixel

| Byte | Content      |
| ----:| ------------ |
| 0    | `'P' (0x50)` |
| 1..3 | `x`          |
| 3..5 | `y`          |
| 5    | `r`          |
| 6    | `g`          |
| 7    | `b`          |



### Get Pixel

| Byte | Content      |
| ----:| ------------ |
| 0    | `'G' (0x47)` |
| 1..3 | `x`          |
| 3..5 | `y`          |
| 5    | ignored      |
| 6    | ignored      |
| 7    | ignored      |

#### Response format

| Byte | Content                                       |
| ----:| --------------------------------------------- |
| 0    | `r`                                           |
| 1    | `g`                                           |
| 2    | `b`                                           |
| 3    | if pixel was inside canvas `1`, otherwise `0` |

## Porting

This doesn't need much SDL3 functionality. The only functionality used is:
- millisecond-precise timing
- opening a window
- copying a texture (rgba buffer) to that window
