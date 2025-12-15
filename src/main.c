#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <SDL3/SDL.h>

#include "cli.h"
#include "canvas.h"
#include "net.h"
#include "common.h"

#define FPS 30
#define MS_PER_FRAME (1000 / (FPS))

#define DEFAULT_PORT 1337

int main(int argc, char *argv[]) {
    struct cli_options opt;
    switch (cli_parse(argc, argv, &opt)) {
        case CLI_OK:
            break;
        case CLI_DONE:
            return 0;
        case CLI_ERR:
            return 1;
        default:
            PANIC("unreachable");
    }

    canvas_start();
    net_start(opt.have_port ? opt.port : DEFAULT_PORT);

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
