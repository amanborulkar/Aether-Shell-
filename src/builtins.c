#include "builtins.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "history.h"

typedef void (*builtin_fn)(const Command *cmd);

static void builtin_cd(const Command *cmd)
{
    const char *target;

    if (cmd->argc > 1) {
        target = cmd->argv[1];
    } else {
        const char *home = getenv("HOME");
        target = home != NULL ? home : "/";
    }

    if (chdir(target) != 0)
        perror("aether: cd");
}

static void builtin_pwd(const Command *cmd)
{
    (void)cmd;
    char buf[PATH_MAX];

    if (getcwd(buf, sizeof buf) == NULL)
        perror("aether: pwd");
    else
        puts(buf);
}

static void builtin_help(const Command *cmd)
{
    (void)cmd;
    puts("Aether Shell V1 built-ins:\n"
         "  cd [dir]    change directory (default: $HOME, or / if unset)\n"
         "  pwd         print the current directory\n"
         "  history     list previous commands (!! repeats the last one)\n"
         "  help        show this message\n"
         "  exit [n]    leave the shell with status n (default: last status)");
}

static void builtin_history(const Command *cmd)
{
    (void)cmd;
    history_print();
}

static const struct {
    const char *name;
    builtin_fn  fn;
} builtins[] = {
    { "cd",      builtin_cd      },
    { "pwd",     builtin_pwd     },
    { "help",    builtin_help    },
    { "history", builtin_history },
};

int builtin_dispatch(Command *cmd)
{
    for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++) {
        if (strcmp(cmd->argv[0], builtins[i].name) == 0) {
            builtins[i].fn(cmd);
            return 1;
        }
    }
    return 0;
}

int builtin_is_exit(const Command *cmd)
{
    return strcmp(cmd->argv[0], "exit") == 0;
}

int builtin_exit_status(const Command *cmd, int last_status)
{
    return cmd->argc > 1 ? atoi(cmd->argv[1]) : last_status;
}