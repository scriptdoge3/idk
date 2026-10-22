#ifndef _STDIO_H
#define _STDIO_H

/* A very small stdio for the Linux 0.01 userland. */

#include <sys/types.h>
#include <stdarg.h>

#define EOF	(-1)
#define BUFSIZ	512

typedef struct {
	int fd;
	int flags;		/* _IOLBF, _IONBF, _IOEOF, _IOERR */
	int pos, len;		/* read side */
	unsigned char buf[BUFSIZ];
} FILE;

#define _IOLBF	1
#define _IONBF	2
#define _IOEOF	4
#define _IOERR	8

extern FILE * stdin, * stdout, * stderr;

int vsnprintf(char * buf, size_t n, const char * fmt, va_list ap);
int snprintf(char * buf, size_t n, const char * fmt, ...);
int sprintf(char * buf, const char * fmt, ...);
int vfprintf(FILE * f, const char * fmt, va_list ap);
int fprintf(FILE * f, const char * fmt, ...);
int printf(const char * fmt, ...);
int fputc(int c, FILE * f);
int fputs(const char * s, FILE * f);
int puts(const char * s);
int fflush(FILE * f);
int fgetc(FILE * f);
char * fgets(char * s, int n, FILE * f);
int feof(FILE * f);
void perror(const char * s);

#define putchar(c)	fputc((c),stdout)
#define putc(c,f)	fputc((c),(f))
#define getchar()	fgetc(stdin)
#define getc(f)		fgetc(f)

#endif
