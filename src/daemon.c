#include <stdio.h>
#include <unistd.h>

#include "server.h"

#define DEFAULT_SOCK_PATH "/tmp/aetherd.sock"

static int usage(void)
{
    fputs("usage: aetherd [-s socket_path]\n", stderr);
    return 1;
}

int main(int argc, char **argv)
{
    const char *path = DEFAULT_SOCK_PATH;
    int opt;

    while ((opt = getopt(argc, argv, "s:")) != -1) {
        if (opt != 's')
            return usage();
        path = optarg;
    }
    if (optind != argc)
        return usage();

    return server_run(path) == 0 ? 0 : 1;
}