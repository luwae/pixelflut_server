#ifndef PFS_CANVAS_H
#define PFS_CANVAS_H

#include "common.h"

void canvas_start(void);
void canvas_stop(void);

// Render current texture to window.
void canvas_draw(void);

int canvas_set_px(const struct pixel *px);
int canvas_get_px(struct pixel *px);

// Iterates through `SDL_Event`s and searches for a quit-event.
int canvas_should_quit(void);

#endif
