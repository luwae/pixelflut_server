# pixelflut server

## Protocol

This server implements a binary protocol. Integers are usually sent in little-endian format (details below).



### Info

Receive general information about the server.

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

The server sends back the following bytes:

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

#### Notes

 - See the general note about send and receive order.



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

#### Notes

 - It is allowed to print a pixel outside of screen bounds. In this case, the server ignores the command.



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

The server sends back the following bytes:

| Byte | Content                                       |
| ----:| --------------------------------------------- |
| 0    | `r`                                           |
| 1    | `g`                                           |
| 2    | `b`                                           |
| 3    | if pixel was inside canvas `1`, otherwise `0` |

#### Notes

 - Even though the last 3 bytes are not used, they still must be sent. The server will not start processing the request until all 8 bytes have been received.
 - It is allowed to request a pixel outside of screen bounds. In this case, the server returns `r = g = b = 0`. The last byte of the answer may be used to determine if this was the case, or if it was a black pixel inside the screen.
 - See the general note about send and receive order.



### Rectangle print

This command first specifies a rectangle `(x, y, w, h)`. Due to space constraints, w and h have possible ranges `0..=4095`.

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

#### Pixel format

The server now expects the client to send `w*h` color values with 4 bytes each. These values are used to fill the rectangle left-to-right and top-to-bottom.

| Byte | Content   |
| ----:| --------- |
| 0    | `r`       |
| 1    | `g`       |
| 2    | `b`       | 
| 3    | undefined |

#### Error handling

 - It is allowed to define a rectangle with `w = 0` and `h = 0`. In this case, The server ignores the command.
 - It is allowed to define a rectangle which escapes screen bounds. In this case, the server still expects `w*h` pixels; the ones outside are then ignored.



### Rectangle get

This command specifies a rectangle `(x, y, w, h)`. Due to space constraints, w and h have possible ranges `0..=4095`.

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

#### Pixel format

The server sends back `w*h` color values with 4 bytes each. The order is left-to-right and top-to-bottom.

The pixel format is the same as for the GET command.

#### Error handling

 - It is allowed to define a rectangle with `w = 0` and `h = 0`. In this case, The server ignores the command.
 - It is allowed to define a rectangle which escapes screen bounds. In this case, the server still sends `w*h` pixels; the ones outside are black (and marked as such in the 4th byte).
 - See the general note about send and receive order.



## Send and receive order

The server may stop processing further commands from a client if its send buffer is full. The send buffer size can be received with the INFO command. For example, for a send buffer size of 1024 the server holds a maximum of 1024 / 4 = 256 color values. This means at most 256 GET commands may be sent to the server before the client must read from the server. Of course, this is a conservative guarantee, as more bytes are likely in-flight or stored in the TCP kernel buffer. However, a client not adhering to this is considered erroneous.
