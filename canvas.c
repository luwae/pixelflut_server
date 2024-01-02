// https://benedicthenshaw.com/soft_render_sdl2.html

#include <stdio.h>
#include <stdlib.h>
#include "SDL.h"

#include "canvas.h"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screen_texture;
unsigned int pixels[1024*1024*4];

void canvas_start() {
    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init: %s\n", SDL_GetError());
        canvas_close();
        exit(1);
    }

    window = SDL_CreateWindow("pixelflut",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            1024, 1024,
            0);
    if (window == NULL) {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        canvas_close();
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("SDL_CreateRenderer: %s\n", SDL_GetError());
        canvas_close();
        exit(1);
    }

    screen_texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
            1024, 1024);
    if (screen_texture == NULL) {
        printf("SDL_CreateTexture: %s\n", SDL_GetError());
        canvas_close();
        exit(1);
    }


    // TODO loop in separate thread
    // TODO efficiency by specifying rect?
    // TODO extra thread
    for (int i = 0; i < 100; i++) {
        // ? SDL_RenderClear(renderer);
        SDL_UpdateTexture(screen_texture, NULL, pixels, 1024*4);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        SDL_Delay(33);
        struct pixel px;
        px.x = i;
        px.y = i;
        px.r = px.g = px.b = 255;
        canvas_set_px(&px);
    }
    // TODO remove this
    canvas_close();
    exit(0);
}

void canvas_close() {
    if (screen_texture)
        SDL_DestroyTexture(screen_texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

void canvas_set_px(const struct pixel *px) {
    unsigned int index = px->x + 1024 * px->y;
    pixels[index] = (px->r << 24) | (px->g << 16) | (px->b << 8) | 0xff;
}
