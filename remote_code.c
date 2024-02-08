#include <string.h>

enum opcode {
    OP_ADD,
};

enum param_type {
    PARAM_NAMED,
    PARAM_IMMEDIATE,
};

struct param {
    enum param_type type;
    long long value; // either immediate value or index into memory map
};

#define MAX_PARAMS 3
struct insn {
    enum opcode op;
    struct param params[MAX_PARAMS];
};

struct mem_map_entry {
    long long off;
    int size; // should be 1, 2, 4, or 8
              // TODO arrays?
};

#define MAX_MEM_SIZE 64
#define MAX_MEM_MAPS 10
#define MAX_INSNS 10
struct execution_state {
    long long num_insns;
    long long next_insn;
    struct insn insns[MAX_INSNS];
    struct mem_map_entry mem_map[MAX_MEM_MAPS];
    unsigned char mem[MAX_MEM_SIZE] __attribute__ ((aligned (8)));
};

long long mem_map_get(struct execution_state *s, struct mem_map_entry *e) {
    long long val = 0;
    for (int i = 0; i < e->size; i++) {
        val |= s->mem[e->off + i] << (i << 3);
    }
    return val;
}

void mem_map_set(struct execution_state *s, struct mem_map_entry *e, long long val) {
    for (int i = 0; i < e->size; i++) {
        s->mem[e->off + i] = (val >> (i << 3)) & 0xff;
    }
}

long long param_get_value(struct execution_state *s, struct param *p) {
    switch (p->type) {
        case PARAM_NAMED:
            return mem_map_get(s, &s->mem_map[p->value]);
        case PARAM_IMMEDIATE:
            return p->value;
    }
    return p->value; // TODO unreachable
}

#define INSN_EXECUTE_DONE 0
int insn_execute(struct execution_state *s) {
    if (s->next_insn == s->num_insns) {
        return INSN_EXECUTE_DONE;
    }
    struct insn *ci = &s->insns[s->next_insn++];
    switch (ci->op) {
        case OP_ADD:
            long long left = param_get_value(s, &ci->params[1]);
            long long right = param_get_value(s, &ci->params[2]);
            mem_map_set(s, &s->mem_map[ci->params[1].value], left + right);
            break;
    }
    return 0;
}

int main() {
    struct execution_state s;
    s.num_insns = 1;
    s.next_insn = 0;
    s.insns[0].op = OP_ADD; // x = x + y
    s.insns[0].params[0].type = PARAM_NAMED;
    s.insns[0].params[0].value = 0;
    s.insns[0].params[1].type = PARAM_NAMED;
    s.insns[0].params[1].value = 0;
    s.insns[0].params[2].type = PARAM_NAMED;
    s.insns[0].params[2].value = 1;
    s.mem_map[0].off = 0;
    s.mem_map[0].size = 4;
    s.mem_map[1].off = 4;
    s.mem_map[1].size = 4;
    memset(s.mem, 0, MAX_MEM_SIZE);
    // x = 5
    s.mem[0] = 5;
    // y = 7
    s.mem[4] = 7;

    return insn_execute(&s);
}
