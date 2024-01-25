# pixelflut server

## Protocol

This server implements a binary protocol. Integers are usually sent in little-endian format (details below).



### Print pixel

| Byte | Content      |
| ----:| ------------ |
| 0    | `'P' (0x50)` |
| 1    | `x[0..=7]`   |
| 2    | `x[8..=15]`  |
| 3    | `y[0..=7]`   |
| 4    | `y[8..=15]   |
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
 - It is allowed to request a pixel outside of screen bounds. In this case, the server returns `r = g = b = 0`. The last byte of the anser may be used to determine if this was the case, or if it was a black pixel inside the screen.
 - It is an error to send a command after a get command without having received its answer. The server may not read further commands from this client until the result of the get command has been read.



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

The pixel format is the same as for the _get pixel_ command.

#### Error handling

 - It is allowed to define a rectangle with `w = 0` and `h = 0`. In this case, The server ignores the command.
 - It is allowed to define a rectangle which escapes screen bounds. In this case, the server still sends `w*h` pixels; the ones outside are black (and marked as such in the 4th byte).
 - It is an error to send a command after a get command without having received its answer. The server may not read further commands from this client until the result of the get command has been read.
