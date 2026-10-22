/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 */
#define memcpy(dest,src,n) ({ \
void * _res = dest; \
int __d0, __d1, __d2; \
__asm__ volatile ("cld;rep;movsb" \
	:"=&D" (__d0),"=&S" (__d1),"=&c" (__d2) \
	:"0" ((long)(_res)),"1" ((long)(src)),"2" ((long) (n)) \
	:"memory"); \
_res; \
})
