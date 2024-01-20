// https://benedicthenshaw.com/soft_render_sdl2.html

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "SDL.h"

#include "common.h"
#include "canvas.h"

#define TEX_SIZE_X 256
#define TEX_SIZE_Y 256
#define SCREEN_SIZE_X 1024
#define SCREEN_SIZE_Y 1024

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screen_texture;
unsigned int pixels[TEX_SIZE_X*TEX_SIZE_Y*4]; // TODO race condition when setting pixels?
pthread_t canvas_thread;

#define CLEANUP_AND_EXIT_IF(error_cond, prefix) do { \
    if (error_cond) {                                \
        printf("%s: %s\n", prefix, SDL_GetError());  \
        canvas_stop();                               \
        exit(1);                                     \
    }                                                \
} while (0)

void canvas_stop(void) {
    if (screen_texture) {
        SDL_DestroyTexture(screen_texture);
        screen_texture = NULL;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
    SDL_Quit();
}

void canvas_draw(void) {
    // ? SDL_RenderClear(renderer);
    SDL_UpdateTexture(screen_texture, NULL, pixels, TEX_SIZE_X*4);
    SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void canvas_start(void) {
    int status = SDL_Init(SDL_INIT_VIDEO);
    CLEANUP_AND_EXIT_IF(status != 0, "SDL_Init");

    window = SDL_CreateWindow("pixelflut",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            SCREEN_SIZE_X, SCREEN_SIZE_Y, 0);
    CLEANUP_AND_EXIT_IF(window == NULL, "SDL_CreateWindow");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    CLEANUP_AND_EXIT_IF(renderer == NULL, "SDL_CreateRenderer");
    
    screen_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
            TEX_SIZE_X, TEX_SIZE_Y);
    CLEANUP_AND_EXIT_IF(screen_texture == NULL, "SDL_CreateTexture");
}

int canvas_set_px(const struct pixel *px) {
    if (px->x >= TEX_SIZE_X || px->y >= TEX_SIZE_Y)
        return 0;
    unsigned int index = px->x + TEX_SIZE_X * px->y;
    pixels[index] = (px->r << 24) | (px->g << 16) | (px->b << 8) | 0xff;
    return 1;
}

int canvas_get_px(struct pixel *px) {
    if (px->x >= TEX_SIZE_X || px->y >= TEX_SIZE_Y) {
        px->r = 0;
        px->g = 0;
        px->b = 0;
        return 0;
    }
    unsigned int index = px->x + TEX_SIZE_X * px->y;
    px->r = (pixels[index] >> 24) & 0xff;
    px->g = (pixels[index] >> 16) & 0xff;
    px->b = (pixels[index] >>  8) & 0xff;
    return 1;
}

int canvas_should_quit(void) {
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT)
            return 1;
    }
    return 0;
}
