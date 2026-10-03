#include "signals.h"

#include <signal.h>
#include <string.h>

#include "lineread.h"

static void on_terminate(int sig)
{
    lineread_restore_terminal();
    raise(sig);
}

static void set_disposition(int sig, void (*handler)(int), int flags)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = handler;
    sa.sa_flags = flags;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void signals_init(void)
{
    set_disposition(SIGINT, SIG_IGN, 0);
    set_disposition(SIGTSTP, SIG_IGN, 0);
    set_disposition(SIGTERM, on_terminate, SA_RESETHAND | SA_NODEFER);
    set_disposition(SIGHUP, on_terminate, SA_RESETHAND | SA_NODEFER);
}

void signals_child_default(void)
{
    set_disposition(SIGINT, SIG_DFL, 0);
    set_disposition(SIGTSTP, SIG_DFL, 0);
}