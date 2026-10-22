/*
 *  signal.c - delivering signals on the way back to user mode.
 *
 * 1991 did this in assembly in system_call.s (ret_from_sys_call); the
 * 64-bit port does the same thing here, called from there.
 *
 * As then: the lowest pending signal is taken; a handler of 0 means the
 * default (exit, except for SIGCHLD), 1 means ignore, anything else is
 * called once and reset to the default. The handler runs on the user
 * stack with a frame the "restorer" (a libc function, given to signal())
 * unwinds:
 *
 *	restorer	<- %rsp; the handler returns into the restorer
 *	rax rcx rdx rsi rdi r8 r9 r10 r11	what the handler may clobber
 *	rip cs rflags rsp ss	where the process was, for an iretq
 *
 * 1991 saved eax, ecx, edx and eflags, the registers a C function was
 * allowed to clobber then, and passed the signal number on the stack;
 * now it's in %rdi. The frame keeps clear of the 128-byte "red zone"
 * under %rsp that 64-bit code may use, and leaves the handler's stack
 * aligned the way the C calling convention wants - so it isn't right
 * under the old %rsp any more, and popping it can't restore %rsp the
 * way 1991's did. The restorer ends with a user-mode iretq instead,
 * which puts back rip, rflags and rsp in one go.
 */
#include <signal.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

extern int do_exit(long code);

void do_signal(struct regs * regs)
{
	unsigned long handler, * sp, segv;
	long sig;
	int bit;

	while (current->signal) {
		for (bit=0 ; !(current->signal & (1<<bit)) ; bit++)
			/* nothing */ ;
		current->signal &= ~(1<<bit);
		sig = bit+1;
		handler = (unsigned long) current->sig_fn[bit];
		if (handler == 1)		/* 1 is ignore */
			continue;
		if (!handler) {			/* 0 is the default */
			if (sig == SIGCHLD)
				continue;
			do_exit(sig);	/* remember to set bit 7 when dumping core */
		}
		current->sig_fn[bit] = NULL;	/* reset signal handler */
		sp = (unsigned long *) (((regs->rsp - 128) & ~15UL) - 15*8);
		segv = current->signal & (1<<(SIGSEGV-1));
		current->signal &= ~(1<<(SIGSEGV-1));
		verify_area(sp,15*8);
		put_fs_long((unsigned long) current->sig_restorer,sp);
		put_fs_long(regs->rax,sp+1);
		put_fs_long(regs->rcx,sp+2);
		put_fs_long(regs->rdx,sp+3);
		put_fs_long(regs->rsi,sp+4);
		put_fs_long(regs->rdi,sp+5);
		put_fs_long(regs->r8,sp+6);
		put_fs_long(regs->r9,sp+7);
		put_fs_long(regs->r10,sp+8);
		put_fs_long(regs->r11,sp+9);
		put_fs_long(regs->rip,sp+10);
		put_fs_long(USER_CS,sp+11);
		put_fs_long(regs->rflags,sp+12);
		put_fs_long(regs->rsp,sp+13);
		put_fs_long(USER_DS,sp+14);
		if (current->signal & (1<<(SIGSEGV-1)))
			do_exit(SIGSEGV);	/* the stack was bad */
		current->signal |= segv;
		regs->rsp = (unsigned long) sp;
		regs->rip = handler;
		regs->rdi = sig;
		return;
	}
}
