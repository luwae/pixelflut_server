#include <string.h>

#include "param.h"
#include "common.h"
#include "canvas.h"

enum opcode {
    OP_ADD,
    OP_GET_WRAP_R,
    OP_GET_WRAP_G,
    OP_GET_WRAP_B,
};

enum param_type {
    PARAM_NAMED,
    PARAM_IMMEDIATE,
};

struct param {
    enum param_type type;
    int value; // either immediate value or index into memory map
};

#define MAX_PARAMS 3
struct insn {
    enum opcode op;
    struct param params[MAX_PARAMS];
};

struct mem_map_entry {
    int off;
    int size; // should be 1 or 4
              // TODO arrays?
};

#define MAX_MEM_MAPS 10
#define MAX_INSNS 10
struct code {
    int num_insns;
    struct insn insns[MAX_INSNS];
    struct mem_map_entry mem_map[MAX_MEM_MAPS];
};

#define MAX_MEM_SIZE 64
struct execution_state {
    struct code *code;
    int next_insn;
    unsigned char mem[MAX_MEM_SIZE] __attribute__ ((aligned (8)));
};

int mem_map_get(struct execution_state *s, int mem_map_index) {
    int val = 0;
    struct mem_map_entry *e = &s->code->mem_map[mem_map_index];
    for (int i = 0; i < e->size; i++) {
        val |= s->mem[e->off + i] << (i << 3);
    }
    return val;
}

void mem_map_set(struct execution_state *s, int mem_map_index, int val) {
    struct mem_map_entry *e = &s->code->mem_map[mem_map_index];
    for (int i = 0; i < e->size; i++) {
        s->mem[e->off + i] = (val >> (i << 3)) & 0xff;
    }
}

int param_get_value(struct execution_state *s, struct param *p) {
    switch (p->type) {
        case PARAM_NAMED:
            return mem_map_get(s, p->value);
        case PARAM_IMMEDIATE:
            return p->value;
    }
    return p->value; // TODO unreachable
}

int insn_execute(struct execution_state *s) {
    if (s->next_insn == s->code->num_insns) {
        return 0;
    }
    struct insn *ci = &s->code->insns[s->next_insn++];
    int left;
    int right;
    struct pixel px;
    unsigned char *colorp;
    switch (ci->op) {
        case OP_ADD:
            left = param_get_value(s, &ci->params[1]);
            right = param_get_value(s, &ci->params[2]);
            mem_map_set(s, ci->params[0].value, left + right);
            break;
        case OP_GET_WRAP_R:
            colorp = &px.r;
            goto selection_done;
        case OP_GET_WRAP_G:
            colorp = &px.g;
            goto selection_done;
        case OP_GET_WRAP_B:
            colorp = &px.b;
selection_done:
            left = param_get_value(s, &ci->params[1]);
            right = param_get_value(s, &ci->params[2]);
            while (left < 0) { left += TEX_SIZE_X; } // TODO mod
            while (left >= TEX_SIZE_X) { left -= TEX_SIZE_X; }
            px.x = left;
            while (right < 0) { right += TEX_SIZE_Y; }
            while (right >= TEX_SIZE_Y) { right -= TEX_SIZE_Y; }
            px.y = right;
            canvas_get_px(&px);
            mem_map_set(s, ci->params[0].value, *colorp);
            break;
    }
    return 1;
}

void execute_full_from(const struct code *c, int x, int y) {
    struct execution_state s;
    s.code = c;
    s.next_insn = 0;
    memset(s.mem, 0, MAX_MEM_SIZE);
    // initialize r, g, b, x, y
    struct pixel px;
    px.x = x;
    px.y = y;
    canvas_get_px(&px);

    mem_map_set(&s, 0, px.r);
    mem_map_set(&s, 1, px.g);
    mem_map_set(&s, 2, px.b);
    mem_map_set(&s, 3, px.x);
    mem_map_set(&s, 4, px.y);

    while (insn_execute(&s))
        ;

    // write back r, g, b
    px.r = mem_map_get(&s, 0);
    px.g = mem_map_get(&s, 1);
    px.b = mem_map_get(&s, 2);
    canvas_set_px(&px);
}

int main() {
    struct code c;
    c.mem_map[0].off = 0; // r
    c.mem_map[0].size = 1;
    c.mem_map[1].off = 1; // g
    c.mem_map[1].size = 1;
    c.mem_map[2].off = 2; // b
    c.mem_map[2].size = 1;
    c.mem_map[3].off = 4; // x
    c.mem_map[3].size = 4;
    c.mem_map[4].off = 8; // y
    c.mem_map[4].size = 4;
    c.num_insns = 2;
    c.insns[0].op = OP_ADD; // x = x - 1
    c.insns[0].params[0].type = PARAM_NAMED;
    c.insns[0].params[0].value = 3;
    c.insns[0].params[1].type = PARAM_NAMED;
    c.insns[0].params[1].value = 3;
    c.insns[0].params[2].type = PARAM_IMMEDIATE;
    c.insns[0].params[2].value = -1;
    c.insns[1].op = OP_GET_WRAP_R; // r = get_wrap_r(x, y)
    c.insns[1].params[0].type = PARAM_NAMED;
    c.insns[1].params[0].value = 0;
    c.insns[1].params[1].type = PARAM_NAMED;
    c.insns[1].params[1].value = 3;
    c.insns[1].params[2].type = PARAM_NAMED;
    c.insns[1].params[2].value = 4;

    struct pixel px;
    px.x = TEX_SIZE_X - 1;
    px.y = 10;
    px.r = 100;
    px.g = 0;
    px.b = 0;
    canvas_set_px(&px);

    execute_full_from(&c, 0, 10);
    execute_full_from(&c, 1, 10);
}

