/*
 * Out-of-line copies of the <string.h> inlines, for the cases where the
 * compiler (or a function pointer) wants a real function. Same trick as
 * the kernel's lib/string.c.
 */
#define extern
#define inline
#include <string.h>
