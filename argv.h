/* 
 * File:   argv.h
 * Author: hadoop
 *
 * Created on 2012年5月13日, 上午9:11
 */

#ifndef ARGV_H
#define	ARGV_H

/* Create a NULL-terminated argv array suitable for passing to execv()
 * from 'cmdline' string.  Characters in the 'ignore' set are treated as white
 * space.  Caller must free with argv_destroy().
 */
char **argv_create(char *cmdline, char *ignore, int *argnum);

/* Destroy an argv array created by argv_create.
 */
void argv_destroy(char **argv);

#define Malloc(size)          wrap_malloc(__FILE__, __LINE__, size)
#define Realloc(item,newsize) wrap_realloc(__FILE__, __LINE__, item, newsize)
char *wrap_malloc(char *file, int line, int size);
char *wrap_realloc(char *file, int line, char *item, int newsize);
void Free(void *ptr);

#ifdef WITH_LSD_NOMEM_ERROR_FUNC
#undef lsd_nomem_error
extern void * lsd_nomem_error(char *file, int line, char *mesg);
#else /* !WITH_LSD_NOMEM_ERROR_FUNC */
#ifndef lsd_nomem_error
#define lsd_nomem_error(file, line, mesg) (NULL)
#endif /* !lsd_nomem_error */
#endif /* !WITH_LSD_NOMEM_ERROR_FUNC */

#endif	/* ARGV_H */

