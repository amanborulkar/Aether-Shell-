#include "history.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

#define WHITESPACE " \t\r\n"

static char *entries[HISTORY_MAX];
static int count;

void history_init(void)
{
    count = 0;
}

void history_add(const char *line)
{
    const char *start = line + strspn(line, WHITESPACE);
    if (*start == '\0')
        return;

    size_t len = strlen(start);
    while (strchr(WHITESPACE, start[len - 1]) != NULL)
        len--;

    char *copy = xmalloc(len + 1);
    memcpy(copy, start, len);
    copy[len] = '\0';

    if (count == HISTORY_MAX) {
        free(entries[0]);
        memmove(entries, entries + 1, (HISTORY_MAX - 1) * sizeof *entries);
        count--;
    }
    entries[count++] = copy;
}

const char *history_get(int index)
{
    if (index < 0 || index >= count)
        return NULL;
    return entries[count - 1 - index];
}

int history_count(void)
{
    return count;
}

const char *history_last(void)
{
    return history_get(0);
}

void history_print(void)
{
    for (int i = 0; i < count; i++)
        printf("%d %s\n", i + 1, entries[i]);
}

void history_free(void)
{
    for (int i = 0; i < count; i++)
        free(entries[i]);
    count = 0;
}