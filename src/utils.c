#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *check_alloc(void *p)
{
    if (p == NULL) {
        perror("aether: out of memory");
        exit(EXIT_FAILURE);
    }
    return p;
}

void *xmalloc(size_t size)
{
    return check_alloc(malloc(size));
}

void *xrealloc(void *ptr, size_t size)
{
    return check_alloc(realloc(ptr, size));
}

char *xstrdup(const char *s)
{
    size_t len = strlen(s) + 1;
    return memcpy(xmalloc(len), s, len);
}