#ifndef PARSER_H
#define PARSER_H

typedef struct {
    char **argv;   /* NULL-terminated */
    int    argc;

    char *in_file;       /* < file                     or NULL */
    char *out_file;      /* > file or >> file or NULL           */
    char *err_file;      /* 2> file or 2>> file or NULL         */
    int   out_append;    /* 1 if >> used, 0 otherwise           */
    int   err_append;    /* 1 if 2>> used, 0 otherwise          */
    int   err_to_out;    /* 1 if 2>&1 used, 0 otherwise         */
    int   err_dup_first; /* 1 if 2>&1 came before any stdout redirection */
} Command;

typedef struct {
    Command **commands;   /* NULL-terminated */
    int       count;
} Pipeline;

Pipeline *parse_line(const char *line);   /* NULL on empty/error */
void      pipeline_free(Pipeline *p);
void      command_free(Command *cmd);

#endif