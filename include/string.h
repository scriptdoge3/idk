#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);

/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal.
 *
 * Modern port: Linus' originals were hand-written string-instruction
 * asm that relied on gcc 1.x letting an asm clobber its own input
 * registers. Current gcc rejects that ("impossible constraints"), so
 * they are plain C here. Same names, same semantics.
 *
 *		(C) 1991 Linus Torvalds
 */

extern inline char * strcpy(char * dest,const char *src)
{
	char * d = dest;

	while ((*d++ = *src++) != '\0')
		/* nothing */ ;
	return dest;
}

extern inline char * strncpy(char * dest,const char *src,int count)
{
	char * d = dest;

	while (count > 0 && (*d = *src) != '\0') {
		d++; src++; count--;
	}
	while (count-- > 0)
		*d++ = '\0';
	return dest;
}

extern inline char * strcat(char * dest,const char * src)
{
	char * d = dest;

	while (*d)
		d++;
	while ((*d++ = *src++) != '\0')
		/* nothing */ ;
	return dest;
}

extern inline char * strncat(char * dest,const char * src,int count)
{
	char * d = dest;

	while (*d)
		d++;
	while (count-- > 0 && (*d = *src++) != '\0')
		d++;
	*d = '\0';
	return dest;
}

extern inline int strcmp(const char * cs,const char * ct)
{
	while (*cs && *cs == *ct) {
		cs++; ct++;
	}
	if (*cs == *ct)
		return 0;
	return (*(unsigned char *)cs < *(unsigned char *)ct) ? -1 : 1;
}

extern inline int strncmp(const char * cs,const char * ct,int count)
{
	for ( ; count > 0 ; count--, cs++, ct++) {
		if (*cs != *ct)
			return (*(unsigned char *)cs < *(unsigned char *)ct) ? -1 : 1;
		if (!*cs)
			break;
	}
	return 0;
}

extern inline char * strchr(const char * s,char c)
{
	for ( ; *s != c ; s++)
		if (!*s)
			return NULL;
	return (char *) s;
}

extern inline char * strrchr(const char * s,char c)
{
	const char * last = NULL;

	do {
		if (*s == c)
			last = s;
	} while (*s++);
	return (char *) last;
}

extern inline int strspn(const char * cs, const char * ct)
{
	const char * p = cs;

	while (*p && strchr(ct,*p))
		p++;
	return p - cs;
}

extern inline int strcspn(const char * cs, const char * ct)
{
	const char * p = cs;

	while (*p && !strchr(ct,*p))
		p++;
	return p - cs;
}

extern inline char * strpbrk(const char * cs,const char * ct)
{
	for ( ; *cs ; cs++)
		if (strchr(ct,*cs))
			return (char *) cs;
	return NULL;
}

extern inline int strlen(const char * s)
{
	const char * p = s;

	while (*p)
		p++;
	return p - s;
}

extern inline char * strstr(const char * cs,const char * ct)
{
	int l = strlen(ct);

	for ( ; *cs ; cs++) {
		if (!strncmp(cs,ct,l))
			return (char *) cs;
	}
	return l ? NULL : (char *) cs;
}

extern char * ___strtok;

extern inline char * strtok(char * s,const char * ct)
{
	char * tok;

	if (!s)
		s = ___strtok;
	if (!s)
		return NULL;
	s += strspn(s,ct);
	if (!*s) {
		___strtok = NULL;
		return NULL;
	}
	tok = s;
	s += strcspn(s,ct);
	if (*s)
		*s++ = '\0';
	else
		s = NULL;
	___strtok = s;
	return tok;
}

extern inline void * memcpy(void * dest,const void * src, int n)
{
	char * d = dest;
	const char * s = src;

	while (n-- > 0)
		*d++ = *s++;
	return dest;
}

extern inline void * memmove(void * dest,const void * src, int n)
{
	char * d = dest;
	const char * s = src;

	if (d < s)
		while (n-- > 0)
			*d++ = *s++;
	else
		while (n-- > 0)
			d[n] = s[n];
	return dest;
}

extern inline int memcmp(const void * cs,const void * ct,int count)
{
	const unsigned char * a = cs, * b = ct;

	for ( ; count > 0 ; count--, a++, b++)
		if (*a != *b)
			return (*a < *b) ? -1 : 1;
	return 0;
}

extern inline void * memchr(const void * cs,char c,int count)
{
	const char * p = cs;

	for ( ; count > 0 ; count--, p++)
		if (*p == c)
			return (void *) p;
	return NULL;
}

extern inline void * memset(void * s,char c,int count)
{
	char * p = s;

	while (count-- > 0)
		*p++ = c;
	return s;
}

#endif
