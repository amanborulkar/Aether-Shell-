#include "parser.h"

#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define INITIAL_CAP 8

typedef enum {
    OP_IN,
    OP_OUT,
    OP_APPEND,
    OP_ERR,
    OP_ERR_APPEND,
    OP_ERR_TO_OUT,
    OP_BOTH
} RedirOp;

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static int is_meta(const char *p)
{
    return p[0] == '<' || p[0] == '>' || (p[0] == '&' && p[1] == '>');
}

static int is_word_end(const char *p)
{
    return *p == '\0' || is_ws(*p) || is_meta(p);
}

static char *read_word(const char **pp)
{
    const char *start = *pp;
    const char *p = start;

    while (!is_word_end(p))
        p++;
    *pp = p;
    if (p == start)
        return NULL;

    size_t len = (size_t)(p - start);
    char *word = xmalloc(len + 1);
    memcpy(word, start, len);
    word[len] = '\0';
    return word;
}

static int match_operator(const char *p, RedirOp *op)
{
    if (p[0] == '<') {
        if (p[1] == '&')
            return -1;
        *op = OP_IN;
        return 1;
    }
    if (p[0] == '>') {
        if (p[1] == '&')
            return -1;
        if (p[1] == '>') {
            if (p[2] == '&')
                return -1;
            *op = OP_APPEND;
            return 2;
        }
        *op = OP_OUT;
        return 1;
    }
    if (p[0] == '&' && p[1] == '>') {
        *op = OP_BOTH;
        return 2;
    }
    if (p[0] == '2' && p[1] == '>') {
        if (p[2] == '&') {
            if (p[3] == '1' && is_word_end(p + 4)) {
                *op = OP_ERR_TO_OUT;
                return 4;
            }
            return -1;
        }
        if (p[2] == '>') {
            if (p[3] == '&')
                return -1;
            *op = OP_ERR_APPEND;
            return 3;
        }
        *op = OP_ERR;
        return 2;
    }
    return 0;
}

static int set_redirect(Command *cmd, RedirOp op, char *name)
{
    switch (op) {
    case OP_IN:
        if (cmd->in_file != NULL)
            return -1;
        cmd->in_file = name;
        return 0;
    case OP_OUT:
    case OP_APPEND:
        if (cmd->out_file != NULL)
            return -1;
        cmd->out_file = name;
        cmd->out_append = op == OP_APPEND;
        return 0;
    case OP_ERR:
    case OP_ERR_APPEND:
        if (cmd->err_file != NULL || cmd->err_to_out)
            return -1;
        cmd->err_file = name;
        cmd->err_append = op == OP_ERR_APPEND;
        return 0;
    case OP_BOTH:
        if (cmd->out_file != NULL || cmd->err_file != NULL || cmd->err_to_out)
            return -1;
        cmd->out_file = name;
        cmd->out_append = 0;
        cmd->err_to_out = 1;
        cmd->err_dup_first = 0;
        return 0;
    case OP_ERR_TO_OUT:
        return -1;
    }
    return -1;
}

static int set_err_to_out(Command *cmd)
{
    if (cmd->err_file != NULL || cmd->err_to_out)
        return -1;
    cmd->err_to_out = 1;
    cmd->err_dup_first = cmd->out_file == NULL;
    return 0;
}

static int fill_command(Command *cmd, const char *p)
{
    int cap = INITIAL_CAP;

    cmd->argv = xmalloc((size_t)cap * sizeof *cmd->argv);
    cmd->argv[0] = NULL;

    for (;;) {
        while (is_ws(*p))
            p++;
        if (*p == '\0')
            return cmd->argc > 0 ? 0 : -1;

        RedirOp op;
        int oplen = match_operator(p, &op);
        if (oplen < 0)
            return -1;

        if (oplen > 0) {
            p += oplen;
            if (op == OP_ERR_TO_OUT) {
                if (set_err_to_out(cmd) != 0)
                    return -1;
                continue;
            }

            while (is_ws(*p))
                p++;
            char *name = read_word(&p);
            if (name == NULL)
                return -1;
            if (set_redirect(cmd, op, name) != 0) {
                free(name);
                return -1;
            }
            continue;
        }

        char *word = read_word(&p);
        if (cmd->argc + 1 >= cap) {
            cap *= 2;
            cmd->argv = xrealloc(cmd->argv, (size_t)cap * sizeof *cmd->argv);
        }
        cmd->argv[cmd->argc++] = word;
        cmd->argv[cmd->argc] = NULL;
    }
}

static Command *parse_segment(const char *segment)
{
    Command *cmd = xmalloc(sizeof *cmd);

    memset(cmd, 0, sizeof *cmd);
    if (fill_command(cmd, segment) != 0) {
        command_free(cmd);
        return NULL;
    }
    return cmd;
}

Pipeline *parse_line(const char *line)
{
    char *copy = xstrdup(line);
    int cap = INITIAL_CAP;
    Pipeline *p = xmalloc(sizeof *p);

    p->commands = xmalloc((size_t)cap * sizeof *p->commands);
    p->commands[0] = NULL;
    p->count = 0;

    char *segment = copy;
    for (;;) {
        char *bar = strchr(segment, '|');
        if (bar != NULL)
            *bar = '\0';

        Command *cmd = parse_segment(segment);
        if (cmd == NULL) {
            pipeline_free(p);
            free(copy);
            return NULL;
        }

        if (p->count + 2 > cap) {
            cap *= 2;
            p->commands = xrealloc(p->commands, (size_t)cap * sizeof *p->commands);
        }
        p->commands[p->count++] = cmd;
        p->commands[p->count] = NULL;

        if (bar == NULL)
            break;
        segment = bar + 1;
    }

    free(copy);
    return p;
}

void command_free(Command *cmd)
{
    if (cmd == NULL)
        return;
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    free(cmd->in_file);
    free(cmd->out_file);
    free(cmd->err_file);
    free(cmd);
}

void pipeline_free(Pipeline *p)
{
    if (p == NULL)
        return;
    for (int i = 0; i < p->count; i++)
        command_free(p->commands[i]);
    free(p->commands);
    free(p);
}