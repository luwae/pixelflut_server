#include <string.h>

#include "param.h"
#include "common.h"
#include "canvas.h"

unsigned int pixels[TEX_SIZE_X*TEX_SIZE_Y*4];

void canvas_start(void) { }

void canvas_stop(void) { }

void canvas_draw(void) { }

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

int canvas_should_quit(void) { return 0; }
