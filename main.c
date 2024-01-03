#include <stdio.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "SDL.h"

#include "common.h"
#include "connection.h"
#include "canvas.h"

#define FPS 30
#define MS_PER_FRAME (1000 / (FPS))

int main() {
    canvas_start();
    net_start();

    while (!canvas_should_quit()) {
        unsigned long long before_drawing = SDL_GetTicks64();
        canvas_draw();
        unsigned long long drawing_time = SDL_GetTicks64() - before_drawing;
        if (drawing_time > MS_PER_FRAME)
            drawing_time = MS_PER_FRAME;
        SDL_Delay(MS_PER_FRAME - drawing_time);
    }

    net_stop();
    canvas_stop();
}
