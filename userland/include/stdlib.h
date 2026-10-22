#ifndef _STDLIB_H
#define _STDLIB_H

#include <sys/types.h>

#define EXIT_SUCCESS	0
#define EXIT_FAILURE	1

extern char ** environ;

void * malloc(size_t n);
void * calloc(size_t n, size_t sz);
void * realloc(void * p, size_t n);
void free(void * p);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atoi(const char * s);
long strtol(const char * s, char ** end, int base);
char * getenv(const char * name);
int setenv(const char * name, const char * value, int overwrite);
char * strdup(const char * s);
char * strerror(int err);
int abs(int x);
int meminfo(unsigned long * buf);	/* total, free, top of memory */

#endif
