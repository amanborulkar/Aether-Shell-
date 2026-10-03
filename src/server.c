#include "server.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define RETRY_DELAY_MS 100

static volatile sig_atomic_t g_stop;
static volatile sig_atomic_t g_wake_wr = -1;

static void on_stop(int sig)
{
    (void)sig;
    int saved_errno = errno;

    g_stop = 1;
    if (g_wake_wr >= 0) {
        ssize_t r = write(g_wake_wr, "x", 1);
        (void)r;
    }
    errno = saved_errno;
}

static int abandon_listener(int fd, const char *path)
{
    close(fd);
    unlink(path);
    return -1;
}

static int open_listener(const char *path)
{
    struct sockaddr_un addr;
    size_t len = strlen(path);

    if (len >= sizeof addr.sun_path) {
        dprintf(STDERR_FILENO, "aetherd: socket path too long: %s\n", path);
        return -1;
    }

    unlink(path);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("aetherd: socket");
        return -1;
    }

    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path, len + 1);

    mode_t old_umask = umask(0177);
    int rc = bind(fd, (struct sockaddr *)&addr, sizeof addr);
    umask(old_umask);
    if (rc < 0) {
        perror("aetherd: bind");
        close(fd);
        return -1;
    }

    if (chmod(path, 0600) < 0) {
        perror("aetherd: chmod");
        return abandon_listener(fd, path);
    }
    if (listen(fd, SOMAXCONN) < 0) {
        perror("aetherd: listen");
        return abandon_listener(fd, path);
    }
    return fd;
}

static int make_wake_pipe(int fds[2])
{
    if (pipe(fds) < 0) {
        perror("aetherd: pipe");
        return -1;
    }
    if (fcntl(fds[1], F_SETFL, O_NONBLOCK) < 0) {
        perror("aetherd: fcntl");
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    return 0;
}

static int install_handlers(void)
{
    static const int stop_sigs[] = { SIGINT, SIGTERM, SIGHUP };
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_stop;
    sigemptyset(&sa.sa_mask);
    for (size_t i = 0; i < sizeof stop_sigs / sizeof stop_sigs[0]; i++) {
        if (sigaction(stop_sigs[i], &sa, NULL) < 0) {
            perror("aetherd: sigaction");
            return -1;
        }
    }

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = SA_NOCLDWAIT | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) < 0) {
        perror("aetherd: sigaction");
        return -1;
    }
    return 0;
}

static _Noreturn void run_session(int conn, int listen_fd, int wake_rd, int wake_wr,
                                  const sigset_t *mask)
{
    static const int sigs[] = { SIGINT, SIGTERM, SIGHUP, SIGCHLD };
    struct sigaction sa;

    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    for (size_t i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
        sigaction(sigs[i], &sa, NULL);
    sigprocmask(SIG_SETMASK, mask, NULL);

    close(listen_fd);
    close(wake_rd);
    close(wake_wr);
    setsid();

    if (dup2(conn, STDIN_FILENO) < 0 || dup2(conn, STDOUT_FILENO) < 0 ||
        dup2(conn, STDERR_FILENO) < 0) {
        dprintf(STDERR_FILENO, "aetherd: dup2: %s\n", strerror(errno));
        _exit(1);
    }
    if (conn > STDERR_FILENO)
        close(conn);

    char arg0[] = "aether";
    char *args[] = { arg0, NULL };

    execvp("aether", args);
    execvp("./aether", args);
    dprintf(STDERR_FILENO, "aetherd: exec failed: %s\n", strerror(errno));
    _exit(127);
}

static void spawn_session(int conn, int listen_fd, int wake_rd, int wake_wr)
{
    sigset_t block, old;

    sigemptyset(&block);
    sigaddset(&block, SIGINT);
    sigaddset(&block, SIGTERM);
    sigaddset(&block, SIGHUP);
    sigprocmask(SIG_BLOCK, &block, &old);

    pid_t pid = fork();
    if (pid == 0)
        run_session(conn, listen_fd, wake_rd, wake_wr, &old);

    sigprocmask(SIG_SETMASK, &old, NULL);
    if (pid < 0)
        perror("aetherd: fork");
    close(conn);
}

static int accept_loop(int listen_fd, int wake_rd, int wake_wr)
{
    struct pollfd fds[2] = {
        { .fd = listen_fd, .events = POLLIN },
        { .fd = wake_rd,   .events = POLLIN },
    };

    while (!g_stop) {
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR)
                continue;
            perror("aetherd: poll");
            return -1;
        }
        if (fds[1].revents & POLLIN)
            break;
        if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            dprintf(STDERR_FILENO, "aetherd: listening socket failed\n");
            return -1;
        }
        if (!(fds[0].revents & POLLIN))
            continue;

        int conn = accept(listen_fd, NULL, NULL);
        if (conn < 0) {
            int err = errno;

            if (err == EINTR || err == ECONNABORTED || err == EAGAIN || err == EWOULDBLOCK)
                continue;
            perror("aetherd: accept");
            if (err == EMFILE || err == ENFILE || err == ENOBUFS || err == ENOMEM) {
                poll(NULL, 0, RETRY_DELAY_MS);
                continue;
            }
            return -1;
        }
        spawn_session(conn, listen_fd, wake_rd, wake_wr);
    }
    return 0;
}

int server_run(const char *sock_path)
{
    int listen_fd = open_listener(sock_path);
    if (listen_fd < 0)
        return -1;

    int wake[2];
    if (make_wake_pipe(wake) < 0)
        return abandon_listener(listen_fd, sock_path);

    int status = -1;

    g_stop = 0;
    g_wake_wr = wake[1];
    if (install_handlers() == 0)
        status = accept_loop(listen_fd, wake[0], wake[1]);

    g_wake_wr = -1;
    close(wake[0]);
    close(wake[1]);
    close(listen_fd);
    unlink(sock_path);
    return status;
}