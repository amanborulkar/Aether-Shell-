#include "lineread.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "history.h"
#include "utils.h"

#define CTRL(c)         ((c) & 0x1f)
#define LINE_INIT_CAP   128
#define ESC_TIMEOUT_MS  50

enum {
    KEY_EOF = -1,
    KEY_NONE = 256,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_DELETE
};

struct linebuf {
    char  *data;
    size_t len;
    size_t cap;
    size_t pos;
};

static struct termios saved_tio;
static volatile sig_atomic_t raw_active;

void lineread_restore_terminal(void)
{
    if (raw_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_tio);
        raw_active = 0;
    }
}

static int enter_raw_mode(void)
{
    struct termios raw;

    if (tcgetattr(STDIN_FILENO, &saved_tio) != 0)
        return 0;

    raw = saved_tio;
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &raw) != 0)
        return 0;
    raw_active = 1;
    return 1;
}

static void out(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t w = write(STDOUT_FILENO, s, n);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        s += w;
        n -= (size_t)w;
    }
}

static int read_byte(void)
{
    unsigned char c;

    for (;;) {
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n == 1)
            return c;
        if (n < 0 && errno == EINTR)
            continue;
        return -1;
    }
}

static int read_byte_timeout(int ms)
{
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    int r;

    do {
        r = poll(&pfd, 1, ms);
    } while (r < 0 && errno == EINTR);
    return r > 0 ? read_byte() : -1;
}

static int read_key(void)
{
    int c = read_byte();
    if (c != 27)
        return c;

    int b = read_byte_timeout(ESC_TIMEOUT_MS);
    if (b != '[' && b != 'O')
        return KEY_NONE;

    int param = 0;
    int after_semi = 0;
    while ((b = read_byte_timeout(ESC_TIMEOUT_MS)) >= 0) {
        if (b >= '0' && b <= '9') {
            if (!after_semi && param < 1000)
                param = param * 10 + (b - '0');
        } else if (b == ';') {
            after_semi = 1;
        } else {
            break;
        }
    }

    switch (b) {
    case 'A': return KEY_UP;
    case 'B': return KEY_DOWN;
    case 'C': return KEY_RIGHT;
    case 'D': return KEY_LEFT;
    case 'H': return KEY_HOME;
    case 'F': return KEY_END;
    case '~':
        switch (param) {
        case 1:
        case 7:  return KEY_HOME;
        case 4:
        case 8:  return KEY_END;
        case 3:  return KEY_DELETE;
        default: return KEY_NONE;
        }
    default:
        return KEY_NONE;
    }
}

static void lb_reserve(struct linebuf *lb, size_t need)
{
    if (need <= lb->cap)
        return;
    while (lb->cap < need)
        lb->cap *= 2;
    lb->data = xrealloc(lb->data, lb->cap);
}

static void lb_insert(struct linebuf *lb, char c)
{
    lb_reserve(lb, lb->len + 2);
    memmove(lb->data + lb->pos + 1, lb->data + lb->pos, lb->len - lb->pos);
    lb->data[lb->pos++] = c;
    lb->data[++lb->len] = '\0';
}

static void lb_delete(struct linebuf *lb, size_t at)
{
    memmove(lb->data + at, lb->data + at + 1, lb->len - at - 1);
    lb->data[--lb->len] = '\0';
}

static void lb_set(struct linebuf *lb, const char *s)
{
    size_t n = strlen(s);

    lb_reserve(lb, n + 1);
    memcpy(lb->data, s, n + 1);
    lb->len = n;
    lb->pos = n;
}

static char *lb_detach(struct linebuf *lb)
{
    char *p = lb->data;

    lb->data = NULL;
    return p;
}

static void refresh(const char *prompt, const struct linebuf *lb)
{
    out("\r", 1);
    out(prompt, strlen(prompt));
    out(lb->data, lb->len);
    out("\x1b[K", 3);

    size_t back = lb->len - lb->pos;
    if (back > 0) {
        char seq[32];
        int n = snprintf(seq, sizeof seq, "\x1b[%zuD", back);
        out(seq, (size_t)n);
    }
}

static char *edit_line(const char *prompt)
{
    struct linebuf lb = { xmalloc(LINE_INIT_CAP), 0, LINE_INIT_CAP, 0 };
    char *draft = NULL;
    char *result = NULL;
    int hist_pos = -1;
    int done = 0;

    lb.data[0] = '\0';
    out(prompt, strlen(prompt));

    while (!done) {
        int key = read_key();

        switch (key) {
        case KEY_EOF:
            done = 1;
            if (lb.len > 0) {
                out("\r\n", 2);
                result = lb_detach(&lb);
            }
            break;

        case '\r':
        case '\n':
            out("\r\n", 2);
            result = lb_detach(&lb);
            done = 1;
            break;

        case CTRL('C'):
            out("^C\r\n", 4);
            result = xstrdup("");
            done = 1;
            break;

        case CTRL('D'):
            if (lb.len == 0) {
                done = 1;
            } else if (lb.pos < lb.len) {
                lb_delete(&lb, lb.pos);
                refresh(prompt, &lb);
            }
            break;

        case 127:
        case CTRL('H'):
            if (lb.pos > 0) {
                int at_end = lb.pos == lb.len;
                lb_delete(&lb, lb.pos - 1);
                lb.pos--;
                if (at_end)
                    out("\b \b", 3);
                else
                    refresh(prompt, &lb);
            }
            break;

        case KEY_DELETE:
            if (lb.pos < lb.len) {
                lb_delete(&lb, lb.pos);
                refresh(prompt, &lb);
            }
            break;

        case CTRL('A'):
        case KEY_HOME:
            lb.pos = 0;
            refresh(prompt, &lb);
            break;

        case CTRL('E'):
        case KEY_END:
            lb.pos = lb.len;
            refresh(prompt, &lb);
            break;

        case KEY_LEFT:
            if (lb.pos > 0) {
                lb.pos--;
                out("\x1b[D", 3);
            }
            break;

        case KEY_RIGHT:
            if (lb.pos < lb.len) {
                lb.pos++;
                out("\x1b[C", 3);
            }
            break;

        case KEY_UP:
            if (hist_pos + 1 < history_count()) {
                if (hist_pos == -1) {
                    free(draft);
                    draft = xstrdup(lb.data);
                }
                hist_pos++;
                lb_set(&lb, history_get(hist_pos));
                refresh(prompt, &lb);
            }
            break;

        case KEY_DOWN:
            if (hist_pos >= 0) {
                hist_pos--;
                lb_set(&lb, hist_pos >= 0 ? history_get(hist_pos) : draft);
                refresh(prompt, &lb);
            }
            break;

        default:
            if (key >= 32 && key != 127 && key < KEY_NONE) {
                char ch = (char)key;
                int at_end = lb.pos == lb.len;
                lb_insert(&lb, ch);
                if (at_end)
                    out(&ch, 1);
                else
                    refresh(prompt, &lb);
            }
            break;
        }
    }

    free(draft);
    free(lb.data);
    return result;
}

static char *read_plain(const char *prompt)
{
    char *line = NULL;
    size_t cap = 0;

    fputs(prompt, stdout);
    fflush(stdout);

    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return NULL;
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    return line;
}

char *lineread(const char *prompt)
{
    if (!isatty(STDIN_FILENO) || !enter_raw_mode())
        return read_plain(prompt);

    char *line = edit_line(prompt);
    lineread_restore_terminal();
    return line;
}