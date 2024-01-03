#ifndef PFS_CANVAS_H
#define PFS_CANVAS_H

#include "common.h"

void canvas_start(void);
void canvas_stop(void);
void canvas_draw(void);
void canvas_set_px(const struct pixel *px);
int canvas_should_quit(void);

#endif
