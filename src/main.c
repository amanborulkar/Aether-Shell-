#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "executor.h"
#include "history.h"
#include "lineread.h"
#include "parser.h"
#include "signals.h"
#include "utils.h"

#define PROMPT "aether> "

static int is_bang_bang(const char *line)
{
    line += strspn(line, " \t");
    if (strncmp(line, "!!", 2) != 0)
        return 0;
    line += 2;
    line += strspn(line, " \t");
    return *line == '\0';
}

int main(void)
{
    int status = 0;

    signals_init();
    history_init();

    puts("Aether Shell V1 — type 'help' for built-ins, 'exit' to quit.");

    for (;;) {
        fflush(stdout);
        char *line = lineread(PROMPT);

        if (line == NULL) {
            putchar('\n');
            status = 0;
            break;
        }

        if (is_bang_bang(line)) {
            const char *last = history_last();
            free(line);
            if (last == NULL) {
                fputs("aether: !!: event not found\n", stderr);
                continue;
            }
            line = xstrdup(last);
            puts(line);
        }

        history_add(line);
        Pipeline *pl = parse_line(line);
        free(line);
        if (pl == NULL)
            continue;

        if (pl->count == 1 && builtin_is_exit(pl->commands[0])) {
            status = builtin_exit_status(pl->commands[0], status);
            pipeline_free(pl);
            break;
        }

        status = execute_pipeline(pl);
        pipeline_free(pl);
    }

    history_free();
    return status;
}