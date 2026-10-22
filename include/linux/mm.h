#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096

/*
 * 64-bit port: the address space (see boot/head.s).
 *
 *	0 .. TASK_SIZE		the current process (each has its own
 *				page tables; 1991 gave each a 64Mb segment)
 *	PAGE_OFFSET ..		all physical memory
 *	KERNEL_BASE ..		the kernel image = physical 0 .. 1Gb
 *
 * get_free_page() hands out PAGE_OFFSET addresses, so the rest of the
 * kernel can go on treating the pages it gets as plain pointers.
 */
#define TASK_SIZE	0x0000008000000000UL	/* 512Gb, one pml4 entry */
#define PAGE_OFFSET	0xffff800000000000UL
#define KERNEL_BASE	0xffffffff80000000UL

/* kernel virtual address <-> physical address */
#define PHYS(v) ((unsigned long) (v) >= KERNEL_BASE ? \
	(unsigned long) (v) - KERNEL_BASE : (unsigned long) (v) - PAGE_OFFSET)
#define VIRT(p) ((unsigned long) (p) + PAGE_OFFSET)
/* low physical memory (video, bios data, the boot loader's leftovers) */
#define LOW(p) ((unsigned long) (p) + KERNEL_BASE)

extern unsigned long get_free_page(void);
extern unsigned long get_free_pages2(void);
extern unsigned long put_page(unsigned long page,unsigned long address);
extern void free_page(unsigned long addr);
extern void mem_init(void);

extern unsigned long buffer_end;

#endif
