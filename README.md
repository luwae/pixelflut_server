# pixelflut server

## Building (SDL2 canvas)
- install SDL2 development files (fedora: `sudo dnf install SDL2-devel`)
- run `make`. Use the makefile to change build directory (default is `./build`)

## Protocol

This server implements a binary protocol. Integers are sent in little-endian format (details below).

### General Remarks

#### Coordinates outside screen bounds

It is allowed to specify coordinates outside screen bounds. In case of reading, pixels outside the screen are interpreted as black (r = g = b = 0). In case of writing, pixels outside the screen are ignored.

#### Zero-size shapes

Rectangles may be defined with `width = 0` or `height = 0`. The resulting command is simply ignored.

#### Undefined bytes

Some commands are smaller than 8 bytes, and the remaining memory is undefined. Nevertheless, all 8 bytes must be sent to the server before the command is processed.

#### Send and receive order

The server may stop processing further commands from a client if its send buffer is full. The send buffer size can be discovered with the INFO command. For example, for a send buffer size of 1024 the server holds a maximum of 1024 / 4 = 256 color values. This means at most 256 GET commands may be sent to the server before the client must read from the server. Of course, this is a conservative guarantee, as more bytes are likely in-flight or stored in the TCP kernel buffer. However, a client not adhering to this is considered erroneous.

It is possible for the client to request more than `SEND_BUFFER_SIZE / 4` colors in a single command, for example with RECTANGLE GET.



### Info

| Byte | Content      |
| ----:| ------------ |
| 0    | `'I' (0x49)` |
| 1    | undefined    |
| 2    | undefined    |
| 3    | undefined    |
| 4    | undefined    |
| 5    | undefined    |
| 6    | undefined    |
| 7    | undefined    |

#### Response format

| Byte | Content                     |
| ----:| --------------------------- |
| 0    | `screen_width[0..=7]`       |
| 1    | `screen_width[8..=15]`      |
| 2    | `screen_width[16..=23]`     |
| 3    | `screen_width[24..=31]`     |
| 4    | `screen_height[0..=7]`      |
| 5    | `screen_height[8..=15]`     |
| 6    | `screen_height[16..=23]`    |
| 7    | `screen_height[24..=31]`    |
| 8    | `recv_buffer_size[0..=7]`   |
| 9    | `recv_buffer_size[8..=15]`  |
| 10   | `recv_buffer_size[16..=23]` |
| 11   | `recv_buffer_size[24..=31]` |
| 12   | `send_buffer_size[0..=7]`   |
| 13   | `send_buffer_size[8..=15]`  |
| 14   | `send_buffer_size[16..=23]` |
| 15   | `send_buffer_size[24..=31]` |



### Print pixel

| Byte | Content      |
| ----:| ------------ |
| 0    | `'P' (0x50)` |
| 1    | `x[0..=7]`   |
| 2    | `x[8..=15]`  |
| 3    | `y[0..=7]`   |
| 4    | `y[8..=15]`  |
| 5    | `r`          |
| 6    | `g`          |
| 7    | `b`          |



### Get Pixel

| Byte | Content      |
| ----:| ------------ |
| 0    | `'G' (0x47)` |
| 1    | `x[0..=7]`   |
| 2    | `x[8..=15]`  |
| 3    | `y[0..=7]`   |
| 4    | `y[8..=15]`  |
| 5    | undefined    |
| 6    | undefined    |
| 7    | undefined    |

#### Response format

| Byte | Content                                       |
| ----:| --------------------------------------------- |
| 0    | `r`                                           |
| 1    | `g`                                           |
| 2    | `b`                                           |
| 3    | if pixel was inside canvas `1`, otherwise `0` |



### Rectangle print

This command first specifies a rectangle `(x, y, w, h)`. Due to space constraints, w and h have possible ranges `0..=4095`.  
The server now expects the client to send `w*h` color values with 4 bytes each. These values are used to fill the rectangle left-to-right and top-to-bottom.

| Byte | Content                                                              |
| ----:| -------------------------------------------------------------------- |
| 0    | `'p' (0x70)`                                                         |
| 1    | `x[0..=7]`                                                           |
| 2    | `x[8..=15]`                                                          |
| 3    | `y[0..=7]`                                                           |
| 4    | `y[8..=15]`                                                          |
| 5    | `w[0..=7]`                                                           |
| 5    | `h[0..=7]`                                                           |
| 7    | from high to low bits: `h[11] h[10] h[9] h[8] w[11] w[10] w[9] w[8]` |

#### Request format

| Byte | Content   |
| ----:| --------- |
| 0    | `r`       |
| 1    | `g`       |
| 2    | `b`       | 
| 3    | undefined |



### Rectangle fill

This command first specifies a rectangle `(x, y, w, h)`. Due to space constraints, w and h have possible ranges `0..=4095`.  
The server now expects the client to send a single color value with 4 bytes. The rectangle is filled with this color left-to-right and top-to-bottom.

| Byte | Content                                                              |
| ----:| -------------------------------------------------------------------- |
| 0    | `'f' (0x66)`                                                         |
| 1    | `x[0..=7]`                                                           |
| 2    | `x[8..=15]`                                                          |
| 3    | `y[0..=7]`                                                           |
| 4    | `y[8..=15]`                                                          |
| 5    | `w[0..=7]`                                                           |
| 5    | `h[0..=7]`                                                           |
| 7    | from high to low bits: `h[11] h[10] h[9] h[8] w[11] w[10] w[9] w[8]` |

#### Request format

| Byte | Content   |
| ----:| --------- |
| 0    | `r`       |
| 1    | `g`       |
| 2    | `b`       | 
| 3    | undefined |



### Rectangle get

This command specifies a rectangle `(x, y, w, h)`. Due to space constraints, w and h have possible ranges `0..=4095`.  
The server sends back `w*h` color values with 4 bytes each. The order is left-to-right and top-to-bottom.

| Byte | Content                                                              |
| ----:| -------------------------------------------------------------------- |
| 0    | `'g' (0x67)`                                                         |
| 1    | `x[0..=7]`                                                           |
| 2    | `x[8..=15]`                                                          |
| 3    | `y[0..=7]`                                                           |
| 4    | `y[8..=15]`                                                          |
| 5    | `w[0..=7]`                                                           |
| 5    | `h[0..=7]`                                                           |
| 7    | from high to low bits: `h[11] h[10] h[9] h[8] w[11] w[10] w[9] w[8]` |

#### Response format

| Byte | Content                                       |
| ----:| --------------------------------------------- |
| 0    | `r`                                           |
| 1    | `g`                                           |
| 2    | `b`                                           |
| 3    | if pixel was inside canvas `1`, otherwise `0` |
