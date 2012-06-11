#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "argv.h"

#define MALLOC_MAGIC    0xf00fbaab
#define MALLOC_PAD_SIZE 16
#define MALLOC_PAD_FILL 0x55

#ifndef NDEBUG
static int memory_alloc = 0;
#endif

/* make a copy of the first word in str and advance str past it */
static char *_nextargv(char **strp, char *ignore) {
    char *str = *strp;
    char *word;
    int len;
    char *cpy = NULL;

    while (*str && (isspace(*str) || strchr(ignore, *str)))
        str++;
    word = str;
    while (*str && !(isspace(*str) || strchr(ignore, *str)))
        str++;
    len = str - word;

    if (len > 0) {
        cpy = (char *) Malloc(len + 1);
        memcpy(cpy, word, len);
        cpy[len] = '\0';
    }

    *strp = str;
    return cpy;
}

/* return number of space seperated words in str */
static int _sizeargv(char *str, char *ignore) {
    int count = 0;

    do {
        while (*str && (isspace(*str) || strchr(ignore, *str)))
            str++;
        if (*str)
            count++;
        while (*str && !(isspace(*str) || strchr(ignore, *str)))
            str++;
    } while (*str);

    return count;
}

/* Create a null-terminated argv array given a command line.
 * Characters in the 'ignore' set are treated like white space. 
 */
char **argv_create(char *cmdline, char *ignore, int *argnum) {
    int argc = _sizeargv(cmdline, ignore);
    char **argv = (char **) Malloc(sizeof (char *) * (argc + 1));
    int i;

    for (i = 0; i < argc; i++) {
        argv[i] = _nextargv(&cmdline, ignore);
        assert(argv[i] != NULL);
    }
    argv[i] = NULL;
    *argnum = argc;

    return argv;
}

/* Destroy a null-terminated argv array.
 */
void argv_destroy(char **argv) {
    int i;

    for (i = 0; argv[i] != NULL; i++)
        Free((void *) argv[i]);
    Free((void *) argv);
}

static int _checkfill(unsigned char *buf, unsigned char fill, int size) {
    (void) &_checkfill; /* Avoid warning "defined but not used" */

    while (size-- > 0)
        if (buf[size] != fill)
            return 0;
    return 1;
}

char *wrap_malloc(char *file, int line, int size) {
    char *new;
    int *p;

    assert(size > 0 && size <= INT_MAX);
    p = (int *) malloc(2 * sizeof (int) +size + MALLOC_PAD_SIZE);
    if (p == NULL)
        return lsd_nomem_error(file, line, "malloc");
    p[0] = MALLOC_MAGIC; /* magic cookie */
    p[1] = size; /* store size in buffer */
#ifndef NDEBUG
    memory_alloc += size;
#endif
    new = (char *) &p[2];
    memset(new, 0, size);
    memset(new + size, MALLOC_PAD_FILL, MALLOC_PAD_SIZE);
    return new;
}

void Free(void *ptr) {
    if (ptr != NULL) {
        int *p = (int *) ptr - 2;
        int size;

        assert(p[0] == MALLOC_MAGIC); /* magic cookie still there? */
        size = p[1];
        assert(_checkfill((unsigned char *) (ptr + size), MALLOC_PAD_FILL, MALLOC_PAD_SIZE));
        memset(p, 0, 2 * sizeof (int) +size + MALLOC_PAD_SIZE);
#ifndef NDEBUG
        memory_alloc -= size;
#endif
        free(p);
    }
}
