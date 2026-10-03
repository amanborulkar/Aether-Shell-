#include "executor.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "signals.h"
#include "utils.h"

static _Noreturn void die_errno(const char *what, int status)
{
    char label[256];

    snprintf(label, sizeof label, "aether: %s", what);
    perror(label);
    _exit(status);
}

static void dup_fd(int from, int to)
{
    if (dup2(from, to) < 0)
        die_errno("dup2", 1);
}

static int write_flags(int append)
{
    return O_WRONLY | O_CREAT | (append ? O_APPEND : O_TRUNC);
}

static void redirect_file(const char *path, int flags, int target)
{
    int fd = open(path, flags, 0644);

    if (fd < 0)
        die_errno(path, 1);
    if (fd != target) {
        dup_fd(fd, target);
        close(fd);
    }
}

static void apply_redirections(const Command *cmd)
{
    if (cmd->in_file != NULL)
        redirect_file(cmd->in_file, O_RDONLY, STDIN_FILENO);
    if (cmd->err_to_out && cmd->err_dup_first)
        dup_fd(STDOUT_FILENO, STDERR_FILENO);
    if (cmd->out_file != NULL)
        redirect_file(cmd->out_file, write_flags(cmd->out_append), STDOUT_FILENO);
    if (cmd->err_to_out && !cmd->err_dup_first)
        dup_fd(STDOUT_FILENO, STDERR_FILENO);
    if (cmd->err_file != NULL)
        redirect_file(cmd->err_file, write_flags(cmd->err_append), STDERR_FILENO);
}

static _Noreturn void child_exec(Command *cmd, int in_fd, int out_fd, int unused_fd,
                                 int run_builtin)
{
    if (in_fd != -1) {
        dup_fd(in_fd, STDIN_FILENO);
        close(in_fd);
    }
    if (out_fd != -1) {
        dup_fd(out_fd, STDOUT_FILENO);
        close(out_fd);
    }
    if (unused_fd != -1)
        close(unused_fd);

    apply_redirections(cmd);
    signals_child_default();

    if (run_builtin && builtin_dispatch(cmd)) {
        fflush(stdout);
        _exit(0);
    }

    execvp(cmd->argv[0], cmd->argv);
    die_errno(cmd->argv[0], 127);
}

static int reap(pid_t pid, int *status)
{
    while (waitpid(pid, status, 0) < 0) {
        if (errno != EINTR) {
            perror("aether: waitpid");
            return -1;
        }
    }
    return 0;
}

static int decode_status(int status)
{
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
}

static int run_pipeline(const Pipeline *p)
{
    int n = p->count;
    pid_t *pids = xmalloc((size_t)n * sizeof *pids);
    int forked = 0;
    int failed = 0;
    int prev_read = -1;
    int pfd[2] = { -1, -1 };
    int result = -1;

    fflush(stdout);

    for (int i = 0; i < n; i++) {
        int last = i == n - 1;

        if (!last && pipe(pfd) < 0) {
            perror("aether: pipe");
            failed = 1;
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("aether: fork");
            failed = 1;
            break;
        }
        if (pid == 0)
            child_exec(p->commands[i], prev_read, pfd[1], pfd[0], n == 1);

        pids[forked++] = pid;
        if (prev_read != -1)
            close(prev_read);
        prev_read = pfd[0];
        if (pfd[1] != -1)
            close(pfd[1]);
        pfd[0] = -1;
        pfd[1] = -1;
    }

    if (prev_read != -1)
        close(prev_read);
    if (pfd[0] != -1)
        close(pfd[0]);
    if (pfd[1] != -1)
        close(pfd[1]);

    if (failed) {
        for (int j = 0; j < forked; j++)
            kill(pids[j], SIGKILL);
    }

    int sigint_seen = 0;
    for (int j = 0; j < forked; j++) {
        int status;
        int ok = reap(pids[j], &status) == 0;

        if (failed)
            continue;
        if (ok && WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
            sigint_seen = 1;
        if (j == n - 1)
            result = ok ? decode_status(status) : -1;
    }
    if (sigint_seen)
        putchar('\n');

    free(pids);
    return result;
}

static int has_redirection(const Command *cmd)
{
    return cmd->in_file != NULL || cmd->out_file != NULL ||
           cmd->err_file != NULL || cmd->err_to_out;
}

int execute_pipeline(Pipeline *p)
{
    if (p->count == 1) {
        Command *cmd = p->commands[0];
        int in_parent = !has_redirection(cmd) || strcmp(cmd->argv[0], "cd") == 0;

        if (in_parent && builtin_dispatch(cmd))
            return 0;
    }

    struct sigaction ign, old_int, old_tstp;

    memset(&ign, 0, sizeof ign);
    ign.sa_handler = SIG_IGN;
    sigemptyset(&ign.sa_mask);
    sigaction(SIGINT, &ign, &old_int);
    sigaction(SIGTSTP, &ign, &old_tstp);

    int status = run_pipeline(p);

    sigaction(SIGINT, &old_int, NULL);
    sigaction(SIGTSTP, &old_tstp, NULL);
    return status;
}