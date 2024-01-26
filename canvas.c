// https://benedicthenshaw.com/soft_render_sdl2.html

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "SDL.h"
#include "SDL2/SDL_ttf.h"

#include "common.h"
#include "canvas.h"
#include "connection.h"

#define TEX_SIZE_X 512
#define TEX_SIZE_Y 512
#define SCREEN_SIZE_X 1024
#define SCREEN_SIZE_Y 1024
#define NET_MASK 0xff

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

#define WHITE (SDL_Color){0xff, 0xff, 0xff, 0xff}

#define FONT_TEXTURE_SIZE 512
#define GLYPH_SIZE 256

struct FontTexture {
    SDL_Rect glyphs[GLYPH_SIZE];
    SDL_Texture * texture;
} fontTexture;

enum FontStatus {
    Success,
    InvalidArg,
    InitFailed,
    FailedOpen,
    FontTooBig,
};

enum FontStatus init_font(SDL_Renderer * renderer) {
    SDL_Surface * text = NULL;
    SDL_Surface * surface = NULL;

    TTF_Font * font = NULL;
    enum FontStatus status = Success;

    if(renderer == NULL) {
        status = InvalidArg;
        goto exit;
    }

    if(TTF_Init()) {
        status = InitFailed;
        goto exit;
    }

    font = TTF_OpenFont("./fonts/SourceCodePro-Regular.ttf", 10);

    if(font == NULL) {
        status = FailedOpen;
        goto exit;
    }

	surface = SDL_CreateRGBSurface(0, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, 32, 0, 0, 0, 0xff);

    /* load all ascii printable characters into the glyph atlas */
    size_t x = 0;
    size_t y = 0;
    for(char c = ' '; c <= '~'; c++) {
        int w, h;

        const char ctext[] = {c, 0};
		text = TTF_RenderUTF8_Blended(font, ctext, WHITE);
		assert(TTF_SizeText(font, ctext, &w, &h) == 0);

        if (x + w >= FONT_TEXTURE_SIZE) {
            x = 0;
            y += h+1;
        }

        if (y + h >= FONT_TEXTURE_SIZE) {
            status = FontTooBig;
            goto exit;
        }

        {
            SDL_Rect * rect;

            rect = fontTexture.glyphs + (size_t)c;
            rect->x = x;
            rect->y = y;
            rect->w = w;
            rect->h = h;

            SDL_BlitSurface(text, NULL, surface, rect);
        }

        x += w;

        SDL_FreeSurface(text);
    }

    /* obtain a texture containing all the glyphs we are interested in */
    fontTexture.texture = SDL_CreateTextureFromSurface(renderer, surface);

    exit:
        SDL_FreeSurface(surface);
        TTF_CloseFont(font);
    return status;
}

void canvas_stop(void) {
    if (fontTexture.texture) {
        SDL_DestroyTexture(fontTexture.texture);
        fontTexture.texture = NULL;
    }
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


void canvas_connection_draw() {
    size_t i = 0;

    /* start drawing at the upper right corner */
    int x = 0;
    int y = 0;

    /* compute the pixels set for an ip */
    unsigned int ip_buckets[NET_MASK + 1];
    memset(ip_buckets, 0, (NET_MASK + 1) * sizeof(*ip_buckets));
    for(i = 0; i < num_conns; i++){
        size_t lsb = (conns + i)->addr.sin_addr.s_addr & NET_MASK;
        ip_buckets[lsb] += (conns + i)->tracker.pixels_set;
    }

    for(i = 0; i < NET_MASK + 1; i++){
        if (ip_buckets[i] == 0) {
            continue;
        }
        char text_buffer[0x100];
        memset(text_buffer, 0, sizeof(text_buffer));

        snprintf(text_buffer, sizeof(text_buffer), "ip: 192.168.2.%03ld %d",
            i, ip_buckets[i]);

        size_t j = 0;
        int max_height = 0;
        for(j = 0; j < sizeof(text_buffer) && text_buffer[j]; j++) {
            SDL_Rect src = fontTexture.glyphs[(size_t)text_buffer[j]];
            SDL_Rect dst = {.x=x, .y=y, .w=src.w, .h=src.h};
            SDL_RenderCopy(renderer, fontTexture.texture, &src, &dst);
            x += dst.w;
            if (dst.h > max_height) {
                max_height = dst.h;
            }
        }
        x = 0;
        y += max_height + 1;
    }
}

void canvas_draw(void) {
    // ? SDL_RenderClear(renderer);
    SDL_UpdateTexture(screen_texture, NULL, pixels, TEX_SIZE_X*4);
    SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
    canvas_connection_draw();
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

    CLEANUP_AND_EXIT_IF(init_font(renderer), "init_font");
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
