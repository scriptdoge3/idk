/*
 * libc.c - just enough C library for the Linux 0.01 userland:
 * stdio, malloc, environment, directories and a few odds and ends.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

char ** environ;

/* ---------------------------------------------------------------- stdio */

static FILE _stdin = { 0, _IOLBF }, _stdout = { 1, _IOLBF }, _stderr = { 2, _IONBF };
FILE * stdin = &_stdin, * stdout = &_stdout, * stderr = &_stderr;

int fflush(FILE * f)
{
	int done = 0, n;

	if (f->fd == 0)
		return 0;
	while (done < f->len) {
		n = write(f->fd, (char *) f->buf + done, f->len - done);
		if (n <= 0) {
			f->flags |= _IOERR;
			f->len = 0;
			return EOF;
		}
		done += n;
	}
	f->len = 0;
	return 0;
}

int fputc(int c, FILE * f)
{
	f->buf[f->len++] = c;
	if (f->len == BUFSIZ || (f->flags & _IONBF) ||
	    ((f->flags & _IOLBF) && c == '\n'))
		fflush(f);
	return (unsigned char) c;
}

int fputs(const char * s, FILE * f)
{
	while (*s)
		fputc(*s++, f);
	return 0;
}

int puts(const char * s)
{
	fputs(s, stdout);
	return fputc('\n', stdout);
}

int fgetc(FILE * f)
{
	int n;

	if (f == stdin)
		fflush(stdout);
	if (f->pos >= f->len) {
		n = read(f->fd, (char *) f->buf, BUFSIZ);
		if (n <= 0) {
			f->flags |= n ? _IOERR : _IOEOF;
			return EOF;
		}
		f->pos = 0;
		f->len = n;
	}
	return f->buf[f->pos++];
}

char * fgets(char * s, int n, FILE * f)
{
	int i = 0, c = 0;

	while (i < n - 1 && (c = fgetc(f)) != EOF) {
		s[i++] = c;
		if (c == '\n')
			break;
	}
	if (i == 0 && c == EOF)
		return NULL;
	s[i] = '\0';
	return s;
}

int feof(FILE * f)
{
	return (f->flags & _IOEOF) != 0;
}

/* A tiny printf core: %d %i %u %x %X %o %s %c %p %%, '-' '0' width, 'l' */
struct out {
	char * buf;
	size_t size, len;
	FILE * f;
};

static void out_c(struct out * o, char c)
{
	if (o->f)
		fputc(c, o->f);
	else if (o->len + 1 < o->size)
		o->buf[o->len] = c;
	o->len++;
}

static void out_pad(struct out * o, const char * s, int len, int width,
	int left, char pad)
{
	int i;

	if (!left && pad == '0' && (*s == '-') && len) {
		out_c(o, *s++);
		len--; width--;
	}
	if (!left)
		for (i = len; i < width; i++)
			out_c(o, pad);
	for (i = 0; i < len; i++)
		out_c(o, s[i]);
	if (left)
		for (i = len; i < width; i++)
			out_c(o, ' ');
}

static int do_printf(struct out * o, const char * fmt, va_list ap)
{
	char tmp[16], * s;
	unsigned long u;
	int left, width, prec, base, neg, len;
	char pad;

	for ( ; *fmt ; fmt++) {
		if (*fmt != '%') {
			out_c(o, *fmt);
			continue;
		}
		left = 0; width = 0; prec = -1; pad = ' ';
		for (fmt++ ; *fmt == '-' || *fmt == '0' ; fmt++)
			if (*fmt == '-')
				left = 1;
			else
				pad = '0';
		if (*fmt == '*') {
			width = va_arg(ap, int);
			fmt++;
		} else
			while (*fmt >= '0' && *fmt <= '9')
				width = width * 10 + *fmt++ - '0';
		if (*fmt == '.') {
			prec = 0;
			for (fmt++ ; *fmt >= '0' && *fmt <= '9' ; fmt++)
				prec = prec * 10 + *fmt - '0';
		}
		while (*fmt == 'l' || *fmt == 'h')
			fmt++;
		base = 0; neg = 0;
		switch (*fmt) {
		case 'c':
			tmp[0] = va_arg(ap, int);
			out_pad(o, tmp, 1, width, left, ' ');
			break;
		case 's':
			s = va_arg(ap, char *);
			if (!s)
				s = "(null)";
			len = strlen(s);
			if (prec >= 0 && prec < len)
				len = prec;
			out_pad(o, s, len, width, left, ' ');
			break;
		case 'd': case 'i':
			u = va_arg(ap, long);
			if ((long) u < 0) {
				neg = 1;
				u = -(long) u;
			}
			base = 10;
			break;
		case 'u': base = 10; u = va_arg(ap, unsigned long); break;
		case 'o': base = 8;  u = va_arg(ap, unsigned long); break;
		case 'p': pad = '0'; width = 8;
		case 'x': case 'X':
			base = 16; u = va_arg(ap, unsigned long); break;
		case '\0':
			fmt--;
			break;
		default:
			out_c(o, *fmt);
		}
		if (base) {
			s = tmp + sizeof tmp;
			do {
				*--s = "0123456789abcdef0123456789ABCDEF"
					[(u % base) + (*fmt == 'X' ? 16 : 0)];
				u /= base;
			} while (u);
			if (neg)
				*--s = '-';
			out_pad(o, s, tmp + sizeof tmp - s, width, left, pad);
		}
	}
	return o->len;
}

int vsnprintf(char * buf, size_t n, const char * fmt, va_list ap)
{
	struct out o = { buf, n, 0, NULL };
	int r = do_printf(&o, fmt, ap);

	if (n)
		buf[o.len < n ? o.len : n - 1] = '\0';
	return r;
}

int snprintf(char * buf, size_t n, const char * fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(buf, n, fmt, ap);
	va_end(ap);
	return r;
}

int sprintf(char * buf, const char * fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vsnprintf(buf, 0x7fffffff, fmt, ap);
	va_end(ap);
	return r;
}

int vfprintf(FILE * f, const char * fmt, va_list ap)
{
	struct out o = { NULL, 0, 0, f };

	return do_printf(&o, fmt, ap);
}

int fprintf(FILE * f, const char * fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vfprintf(f, fmt, ap);
	va_end(ap);
	return r;
}

int printf(const char * fmt, ...)
{
	va_list ap;
	int r;

	va_start(ap, fmt);
	r = vfprintf(stdout, fmt, ap);
	va_end(ap);
	return r;
}

static const char * errlist[] = {
	"Success", "Operation not permitted", "No such file or directory",
	"No such process", "Interrupted system call", "I/O error",
	"No such device or address", "Argument list too long",
	"Exec format error", "Bad file number", "No child processes",
	"Try again", "Out of memory", "Permission denied", "Bad address",
	"Block device required", "Device or resource busy", "File exists",
	"Cross-device link", "No such device", "Not a directory",
	"Is a directory", "Invalid argument", "File table overflow",
	"Too many open files", "Not a typewriter", "Text file busy",
	"File too large", "No space left on device", "Illegal seek",
	"Read-only file system", "Too many links", "Broken pipe",
	"Math argument out of domain", "Math result not representable",
	"Resource deadlock would occur", "File name too long",
	"No record locks available", "Function not implemented",
	"Directory not empty"
};

char * strerror(int err)
{
	static char buf[24];

	if (err >= 0 && err < (int) (sizeof errlist / sizeof errlist[0]))
		return (char *) errlist[err];
	snprintf(buf, sizeof buf, "Error %d", err);
	return buf;
}

void perror(const char * s)
{
	if (s && *s)
		fprintf(stderr, "%s: ", s);
	fprintf(stderr, "%s\n", strerror(errno));
}

/* --------------------------------------------------------------- malloc */

/* K&R style first-fit allocator on top of sbrk() */
typedef union header {
	struct {
		union header * next;
		size_t size;		/* in units of sizeof(Header) */
	} s;
	long align;
} Header;

static Header base, * freep;

void free(void * ap)
{
	Header * bp, * p;

	if (!ap)
		return;
	bp = (Header *) ap - 1;
	for (p = freep; !(bp > p && bp < p->s.next); p = p->s.next)
		if (p >= p->s.next && (bp > p || bp < p->s.next))
			break;
	if (bp + bp->s.size == p->s.next) {
		bp->s.size += p->s.next->s.size;
		bp->s.next = p->s.next->s.next;
	} else
		bp->s.next = p->s.next;
	if (p + p->s.size == bp) {
		p->s.size += bp->s.size;
		p->s.next = bp->s.next;
	} else
		p->s.next = bp;
	freep = p;
}

static Header * morecore(size_t nu)
{
	char * cp;
	Header * up;

	if (nu < 1024)
		nu = 1024;
	cp = sbrk(nu * sizeof(Header));
	if (cp == (char *) -1)
		return NULL;
	up = (Header *) cp;
	up->s.size = nu;
	free(up + 1);
	return freep;
}

void * malloc(size_t n)
{
	Header * p, * prev;
	size_t nunits = (n + sizeof(Header) - 1) / sizeof(Header) + 1;

	if (!(prev = freep)) {
		base.s.next = freep = prev = &base;
		base.s.size = 0;
	}
	for (p = prev->s.next; ; prev = p, p = p->s.next) {
		if (p->s.size >= nunits) {
			if (p->s.size == nunits)
				prev->s.next = p->s.next;
			else {
				p->s.size -= nunits;
				p += p->s.size;
				p->s.size = nunits;
			}
			freep = prev;
			return p + 1;
		}
		if (p == freep && !(p = morecore(nunits))) {
			errno = ENOMEM;
			return NULL;
		}
	}
}

void * calloc(size_t n, size_t sz)
{
	void * p = malloc(n * sz);

	if (p)
		memset(p, 0, n * sz);
	return p;
}

void * realloc(void * old, size_t n)
{
	void * p;
	size_t have;

	if (!old)
		return malloc(n);
	have = (((Header *) old - 1)->s.size - 1) * sizeof(Header);
	if (have >= n)
		return old;
	if ((p = malloc(n))) {
		memcpy(p, old, have);
		free(old);
	}
	return p;
}

char * strdup(const char * s)
{
	char * p = malloc(strlen(s) + 1);

	if (p)
		strcpy(p, s);
	return p;
}

/* ---------------------------------------------------------- misc stdlib */

void exit(int status)
{
	fflush(stdout);
	fflush(stderr);
	_exit(status);
}

void abort(void)
{
	kill(getpid(), 6);
	_exit(127);
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

long strtol(const char * s, char ** end, int base)
{
	long v = 0;
	int neg = 0, d;

	while (*s == ' ' || *s == '\t')
		s++;
	if (*s == '-' || *s == '+')
		neg = (*s++ == '-');
	if ((base == 0 || base == 16) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
		base = 16;
	} else if (base == 0)
		base = (*s == '0') ? 8 : 10;
	for ( ; ; s++) {
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (*s >= 'a' && *s <= 'z')
			d = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'Z')
			d = *s - 'A' + 10;
		else
			break;
		if (d >= base)
			break;
		v = v * base + d;
	}
	if (end)
		*end = (char *) s;
	return neg ? -v : v;
}

int atoi(const char * s)
{
	return strtol(s, NULL, 10);
}

char * getenv(const char * name)
{
	char ** e;
	int n = strlen(name);

	for (e = environ; e && *e; e++)
		if (!strncmp(*e, name, n) && (*e)[n] == '=')
			return *e + n + 1;
	return NULL;
}

int setenv(const char * name, const char * value, int overwrite)
{
	static int ours = 0;
	char ** e, ** ne, * s;
	int n = strlen(name), count = 0;

	for (e = environ; e && *e; e++, count++)
		if (!strncmp(*e, name, n) && (*e)[n] == '=') {
			if (!overwrite)
				return 0;
			break;
		}
	if (!(s = malloc(n + strlen(value) + 2)))
		return -1;
	strcpy(s, name);
	s[n] = '=';
	strcpy(s + n + 1, value);
	if (e && *e) {
		*e = s;
		return 0;
	}
	if (!(ne = malloc((count + 2) * sizeof(char *))))
		return -1;
	if (count)
		memcpy(ne, environ, count * sizeof(char *));
	ne[count] = s;
	ne[count + 1] = NULL;
	if (ours)
		free(environ);
	environ = ne;
	ours = 1;
	return 0;
}

/* ----------------------------------------------------------- directories */

DIR * opendir(const char * name)
{
	struct stat st;
	DIR * d;
	int fd;

	if ((fd = open(name, O_RDONLY)) < 0)
		return NULL;
	if (fstat(fd, &st) < 0 || !S_ISDIR(st.st_mode)) {
		close(fd);
		errno = ENOTDIR;
		return NULL;
	}
	if (!(d = malloc(sizeof(DIR)))) {
		close(fd);
		return NULL;
	}
	d->fd = fd;
	return d;
}

struct dirent * readdir(DIR * d)
{
	struct {
		unsigned short ino;
		char name[NAME_MAX];
	} raw;

	while (read(d->fd, (char *) &raw, sizeof raw) == sizeof raw) {
		if (!raw.ino)
			continue;
		d->ent.d_ino = raw.ino;
		memcpy(d->ent.d_name, raw.name, NAME_MAX);
		d->ent.d_name[NAME_MAX] = '\0';
		return &d->ent;
	}
	return NULL;
}

int closedir(DIR * d)
{
	int r = close(d->fd);

	free(d);
	return r;
}
