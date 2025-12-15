#include <stdio.h>
#include <string.h>
#include "cli.h"

static bool parse_u64(char *s, uint64_t *out) {
    if (s[0] == '\0') {
        return false;
    }
    // number 0
    if (s[0] == '0' && s[1] == '\0') {
        *out = 0;
        return true;
    }
    // 1-9, followed by 0-9*
    if (s[0] < '1' || s[0] > '9') {
        return false;
    }
    uint64_t res = s[0] - '0';
    uint64_t res_old;
    s++;
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        res_old = res;
        res = res * 10 + (*s - '0');
        if (res_old > res) {
            // overflow
            return false;
        }
        s++;
    }
    *out = res;
    return true;
}

static void cli_print_help(void);

enum cli_result cli_parse(int argc, char *argv[], struct cli_options *opt) {
    memset(opt, 0, sizeof(*opt));
    for (int idx = 1; idx < argc; idx++) {
        const char *s = argv[idx];
        if (strcmp(s, "--port") == 0) {
            idx++;
            if (opt->have_port) {
                printf("duplicate --port\n");
                return CLI_ERR;
            }
            if (idx >= argc) {
                printf("missing value for option --port\n");
                return CLI_ERR;
            }
            uint64_t parsed;
            if (!parse_u64(argv[idx], &parsed)) {
                printf("could not parse port number from '%s'\n", argv[idx]);
                return CLI_ERR;
            }
            if (parsed > 0xffff) {
                printf("port overflow: should be 16-bit\n");
                return CLI_ERR;
            }
            opt->have_port = true;
            opt->port = (uint16_t) parsed;
        } else if (strcmp(s, "--help") == 0) {
            cli_print_help();
            return CLI_DONE;
        } else {
            printf("unknown argument: `%s`\n", s);
            return CLI_ERR;
        }
    }
    return CLI_OK;
}

static void cli_print_help(void) {
    printf("command line usage:\n");
    printf("- `--port <port>`: open connection on specified port\n");
}
