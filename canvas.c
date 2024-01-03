// https://benedicthenshaw.com/soft_render_sdl2.html

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "SDL.h"

#include "common.h"
#include "canvas.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screen_texture;
unsigned int pixels[1024*1024*4]; // TODO race condition when setting pixels?
pthread_t canvas_thread;

#define CLEANUP_AND_EXIT_IF(error_cond, prefix) do { \
    if (error_cond) {                                \
        printf("%s: %s\n", prefix, SDL_GetError());  \
        canvas_close();                              \
        exit(1);                                     \
    }                                                \
} while (0)

void canvas_close(void) {
    if (screen_texture)
        SDL_DestroyTexture(screen_texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

void *canvas_thread_main(void *arg) {
    (void) arg; // suppress compiler warning
    printf("started canvas thread\n");
    // TODO efficiency by specifying rect?
    while (!should_quit) {
        // TODO event handling -> closing window should set should_quit for main thread to exit
        // ? SDL_RenderClear(renderer);
        SDL_UpdateTexture(screen_texture, NULL, pixels, 1024*4);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(33);
    }
    printf("stopping canvas thread\n");
    canvas_close();
    return NULL;
}

void canvas_start(void) {
    for (int i = 0; i < 1024*1024; i++) {
        pixels[i] = 0xff;
    }

    int status = SDL_Init(SDL_INIT_VIDEO);
    CLEANUP_AND_EXIT_IF(status != 0, "SDL_Init");

    window = SDL_CreateWindow("pixelflut",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1024, 1024, 0);
    CLEANUP_AND_EXIT_IF(window == NULL, "SDL_CreateWindow");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    CLEANUP_AND_EXIT_IF(renderer == NULL, "SDL_CreateRenderer");
    
    screen_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
            1024, 1024);
    CLEANUP_AND_EXIT_IF(screen_texture == NULL, "SDL_CreateTexture");

    status = pthread_create(&canvas_thread, NULL, canvas_thread_main, NULL);
    if (status != 0) {
        printf("pthread_create: %d\n", status);
        canvas_close();
        exit(1);
    }
}

void canvas_set_px(const struct pixel *px) {
    unsigned int index = px->x + 1024 * px->y;
    pixels[index] = (px->r << 24) | (px->g << 16) | (px->b << 8) | 0xff;
}
