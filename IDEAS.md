# Ensuring fairness between clients

We iterate over all the clients. In each iteration, each client is allowed:
- up to 1 `read()` syscall
  - they are made as late as possible, to maximize read size (still every round or only when there isn't a complete command in the buffer anymore?)
- up to 1 `write()` syscall
  - do this every iteration? -> May be wasteful, but probably not if we have many contiguous send commands
  - do this if the buffer is full or with timeout? -> complicated; unsure what the timeout should be; This is TCP's task
- up to 1 drawn pixel
  - perhaps this is too little, as a client may receive up to 256 pixels per iteration, but only draw one. So write-only clients get little bandwidth compared to read-heavy clients. Perhaps this is unavoidable. We could also use a fixed limit `n > 1`, perhaps `BUFFER_SIZE/PRINT_COMMAND_SIZE` (128)
  - or: iterate over all connections, find the minimum of drawn pixels in the next iteration, and then use this for every connection.
  - Idea: allow users to waive their right on drawing a pixel and store it for later, so that the client may draw many pixels undisturbed. This only works if we collect these pixels in a separate buffer beforehand. Otherwise we may wait arbitrarily long times in `read()`.

# Fill rect with solid color command

Self-explanatory. Send rect as in RECTANGLE GET or RECTANGLE PRINT, and then send a single 4-byte color value

# Remote convolution matrix

Client defines a convolution matrix, that he may subsequently apply to any single or to a rectangle of pixels. This may also need screen snapshotting functionality.

# Remote code execution

Generalized version of the remote convolution matrix. Client may define code (in specialized instruction set) where pixel values can be manipulated based on the current pixel value and the values of its neighbors. Later, the client specifies which code it wants to execute for which pixel coordinate.

The goal is to create a protocol where the client never has to receive pixel values to modify them.

Make sure this does not get too general, otherwise we have to prove there are no infinite loops or something.

# Split screen mode

There are 4 clients, and everyone gets their own quarter of the screen (no disturbance). Screen size must be reported as the new size for the clients, so they may draw accordingly. Also it should be known to the clients that it is split screen mode (they don't need to "reserve" pixels, see idea above).

The split screen must be pinned to an ip, not a single connection!
