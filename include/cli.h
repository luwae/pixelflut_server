#ifndef PFS_CLI_H
#define PFS_CLI_H

#include <stdint.h>
#include <stdbool.h>

enum cli_result {
    CLI_OK,
    CLI_ERR,
    CLI_DONE,
};

struct cli_options {
    bool have_port;
    uint16_t port;
};

enum cli_result cli_parse(int argc, char *argv[], struct cli_options *opt);

#endif
