#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int builtin_dispatch(Command *cmd);  /* 1 if handled, 0 if not a builtin */

int builtin_is_exit(const Command *cmd);
int builtin_exit_status(const Command *cmd, int last_status);

#endif