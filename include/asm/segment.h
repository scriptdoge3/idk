/*
 * Moving data between the kernel and the current process.
 *
 * 1991: %fs held the process's data segment, so these read and wrote
 * through %fs. 64-bit port: the process is mapped at its own addresses
 * (0 .. TASK_SIZE) in the page tables that are loaded while it runs, so
 * the kernel uses its pointers directly. What %fs's segment limit used
 * to guarantee - that a process can't make the kernel touch kernel
 * memory - is now an explicit check. Code that really passes kernel
 * pointers (printk, the init task before it execs) sets fs_kernel,
 * the equivalent of loading %fs with the kernel data segment.
 *
 * A bad address gets the process a SIGSEGV (reads return 0, writes are
 * dropped), much like the segment exception it got in 1991.
 *
 * The "long" ones move an unsigned long, now 64 bits; the "int" ones
 * are for user ints (1991's long).
 */
#ifndef _SEGMENT_H
#define _SEGMENT_H

#include <linux/sched.h>

extern void fs_fault(void);

#define FS_OK(addr,size) (current->fs_kernel || \
	((unsigned long) (addr) < TASK_SIZE && \
	 (unsigned long) (addr) + (size) <= TASK_SIZE))

#define _GET_FS(type,addr) do { \
	if (!FS_OK(addr,sizeof(type))) { \
		fs_fault(); \
		return 0; \
	} \
	return *(volatile type *) (addr); \
} while (0)

#define _PUT_FS(type,val,addr) do { \
	if (!FS_OK(addr,sizeof(type))) \
		fs_fault(); \
	else \
		*(volatile type *) (addr) = (val); \
} while (0)

static inline unsigned char get_fs_byte(const char * addr)
{
	_GET_FS(unsigned char,addr);
}

static inline unsigned short get_fs_word(const unsigned short *addr)
{
	_GET_FS(unsigned short,addr);
}

static inline unsigned int get_fs_int(const unsigned int *addr)
{
	_GET_FS(unsigned int,addr);
}

static inline unsigned long get_fs_long(const unsigned long *addr)
{
	_GET_FS(unsigned long,addr);
}

static inline void put_fs_byte(char val,char *addr)
{
	_PUT_FS(char,val,addr);
}

static inline void put_fs_word(short val,short * addr)
{
	_PUT_FS(short,val,addr);
}

static inline void put_fs_int(unsigned int val,unsigned int * addr)
{
	_PUT_FS(unsigned int,val,addr);
}

static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
	_PUT_FS(unsigned long,val,addr);
}

#endif
