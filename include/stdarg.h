#ifndef _STDARG_H
#define _STDARG_H

/*
 * Modern port: the original computed argument addresses by hand from
 * &LASTARG, which an optimising compiler is free to break. Use the
 * compiler builtins instead (on i386 va_list is still just a char *).
 */
typedef __builtin_va_list va_list;

#define va_start(AP, LASTARG)	__builtin_va_start(AP, LASTARG)
#define va_end(AP)		__builtin_va_end(AP)
#define va_arg(AP, TYPE)	__builtin_va_arg(AP, TYPE)

#endif /* _STDARG_H */
