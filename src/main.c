#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <SDL3/SDL.h>

#include "canvas.h"
#include "net.h"

#define FPS 30
#define MS_PER_FRAME (1000 / (FPS))

int main() {
    canvas_start();
    net_start();

    unsigned long long start = SDL_GetTicks();
    unsigned long long framecounter = 0;
    while (!canvas_should_quit()) {
        canvas_draw();
        unsigned long long now = SDL_GetTicks();
        unsigned long long next_frame_time = start + (++framecounter * MS_PER_FRAME);
        if (now < next_frame_time) {
            SDL_Delay(next_frame_time - now);
        }
    }

    net_stop();
    canvas_stop();
}
