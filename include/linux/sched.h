#ifndef _SCHED_H
#define _SCHED_H

#define NR_TASKS 64
#define HZ 100

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

#ifndef NULL
#define NULL ((void *) 0)
#endif

/* segment selectors, see the gdt in boot/head.s */
#define KERNEL_CS	0x08
#define KERNEL_DS	0x10
#define USER_CS		0x1b
#define USER_DS		0x23
#define TSS_SEL		0x28

/*
 * 64-bit port: each task has a two-page block: the task_struct at the
 * bottom, its kernel stack above it (1991: one page, and a tss for the
 * hardware to switch with).
 */
#define TASK_BLOCK	(2*PAGE_SIZE)

extern int copy_page_tables(unsigned long * from, unsigned long * to);
extern void free_page_tables(unsigned long * pml4);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);
extern void panic(const char * str) __attribute__((noreturn));
extern int tty_write(unsigned minor,char * buf,int count);

typedef long (*fn_ptr)();

/*
 * The registers as the low-level entry code (kernel/system_call.s)
 * saves them on the kernel stack: one of these sits at the top of the
 * kernel stack while a process is in the kernel.
 */
struct regs {
	unsigned long r15,r14,r13,r12,r11,r10,r9,r8;
	unsigned long rbp,rdi,rsi,rdx,rcx,rbx,rax;
	unsigned long pad;		/* keeps the C stack 16-byte aligned */
	unsigned long error_code;	/* pushed by the cpu, or 0 */
	unsigned long rip,cs,rflags,rsp,ss;
};

/* the one tss: only rsp0 (and a stack for double faults) matter */
struct tss_struct {
	unsigned int	reserved0;
	unsigned long	rsp0,rsp1,rsp2;
	unsigned long	reserved1;
	unsigned long	ist[7];
	unsigned long	reserved2;
	unsigned short	reserved3;
	unsigned short	iomap_base;
} __attribute__((packed));

struct task_struct {
/* these are hardcoded - don't touch (kernel/system_call.s) */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */
	long counter;
	long priority;
	long signal;
	unsigned long ksp;	/* kernel stack pointer while switched out */
	unsigned long cr3;	/* physical address of the page tables */
	fn_ptr sig_restorer;
	fn_ptr sig_fn[32];
/* various fields */
	int exit_code;
	unsigned long end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
	unsigned short fs_kernel;	/* "fs" may point at kernel data */
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* 64-bit port: these replace the ldt and the tss */
	unsigned long * pml4;	/* page tables, kernel virtual address */
	unsigned char fxsave[512] __attribute__((aligned(16)));
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Task 0 never leaves the kernel: it is the idle task
 * and runs on the kernel's own page tables.
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,0,0,NULL,{(fn_ptr) 0,}, \
/* ec,brk... */	0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0,1, \
/* fs info */	-1,0133,NULL,NULL,0, \
/* filp */	{NULL,}, \
/* pml4 */	pml4, \
}

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;
extern long startup_time;
extern struct tss_struct tss;

#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void sleep_on(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);

/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 *
 * 64-bit port: there is no hardware task switch any more. switch_task
 * (kernel/system_call.s) saves the callee-saved registers and flags on
 * the old kernel stack, takes the new one, loads cr3 and the tss's rsp0,
 * and sets TS unless the new task was the last to use the fpu.
 */
extern void switch_task(struct task_struct * next);

#define switch_to(n) {\
	if (task[n] != current) \
		switch_task(task[n]); \
}

#define PAGE_ALIGN(n) (((n)+0xfff)&~0xfffUL)

#endif
