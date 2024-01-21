# pixelflut server

## Protocol

This server implements a binary protocol.

### Print pixel

| Byte | Content     |
| ----:| ----------- |
| 0    | `'P'`       |
| 1    | `x[0..=7]`  |
| 2    | `x[8..=15]` |
| 3    | `x[0..=7]`  |
| 4    | `x[8..=15]` |
| 5    | `r`         |
| 6    | `g`         |
| 7    | `b`         |
