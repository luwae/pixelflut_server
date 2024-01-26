#ifndef PFS_CANVAS_H
#define PFS_CANVAS_H

#include "common.h"

void canvas_start(void);
void canvas_stop(void);
void canvas_draw(void);
int canvas_set_px(const struct pixel *px);
int canvas_get_px(struct pixel *px);
unsigned int canvas_get_width();
unsigned int canvas_get_height();
int canvas_should_quit(void);

#endif
