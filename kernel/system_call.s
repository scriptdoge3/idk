/*
 *  system_call.s  contains the system-call low-level handling routines.
 * This also contains the timer-interrupt handler, as some of the code is
 * the same. The hd-interrupt is also here.
 *
 * NOTE: This code handles signal-recognition, which happens every time
 * after a timer-interrupt and after each system call. Ordinary interrupts
 * don't handle signal-recognition, as that would clutter them up totally
 * unnecessarily.
 *
 * 64-bit port: the stack layout is struct regs (see asm/regs.inc). A
 * system call still comes in by int $0x80 with its number in %rax and
 * up to three arguments in %rbx, %rcx, %rdx; they are handed to the C
 * function in the 64-bit calling convention's %rdi, %rsi, %rdx, with a
 * pointer to the saved registers as a fourth argument for the calls
 * that need them (fork, execve). Signal delivery moved to C (signal.c's
 * do_signal), and so did sys_fork and sys_execve.
 */

	.include "asm/regs.inc"

nr_system_calls = 68

.globl _system_call,_timer_interrupt,_hd_interrupt
.globl _switch_task,_ret_from_fork,_kernel_thread_start

.p2align 3
bad_sys_call:
	movq $-1,RAX(%rsp)
	jmp ret_from_sys_call
.p2align 3
reschedule:
	call _schedule
	jmp ret_from_sys_call
.p2align 3
_system_call:
	pushq $0		# no error code
	pushq $0		# alignment
	SAVE_REGS
	cmpq $nr_system_calls-1,%rax
	ja bad_sys_call
	movq %rbx,%rdi		# the arguments
	movq %rcx,%rsi
	movq %rsp,%rcx		# and the saved registers
	call *_sys_call_table(,%rax,8)
	movq %rax,RAX(%rsp)
	movq _current,%rax
	cmpq $0,state(%rax)		# state
	jne reschedule
	cmpq $0,counter(%rax)		# counter
	je reschedule
ret_from_sys_call:
	testb $3,CS(%rsp)		# was old code segment supervisor
	je 3f				# mode? If so - don't check signals
	movq _current,%rax
	cmpq $0,signal(%rax)
	je 3f
	movq %rsp,%rdi
	call _do_signal
3:	RESTORE_REGS
	addq $16,%rsp			# alignment, error code
	iretq

.p2align 3
_timer_interrupt:
	pushq $0
	pushq $0
	SAVE_REGS
	incq _jiffies
	movb $0x20,%al		# EOI to interrupt controller #1
	outb %al,$0x20
	movl CS(%rsp),%edi
	andl $3,%edi		# %edi is CPL (0 or 3, 0=supervisor)
	call _do_timer		# 'do_timer(long CPL)' does everything from
	jmp ret_from_sys_call	# task switching to accounting ...

.p2align 3
_hd_interrupt:
	pushq $0
	pushq $0
	SAVE_REGS
	movb $0x20,%al
	outb %al,$0x20		# EOI to interrupt controller #1
	jmp 1f			# give port chance to breathe
1:	jmp 1f
1:	outb %al,$0xA0		# same to controller #2
	xorl %eax,%eax
	xchgq _do_hd,%rax	# take the handler, leaving NULL behind
	testq %rax,%rax
	jne 1f
	movq $_unexpected_hd_interrupt,%rax
1:	call *%rax		# "interesting" way of handling intr.
	RESTORE_REGS
	addq $16,%rsp
	iretq

/*
 * void switch_task(struct task_struct * next)
 *
 * What the 386 did in hardware on a jump to a tss. Everything the C code
 * expects to survive a call is pushed on the old task's kernel stack
 * (rflags too: sleep_on() can be called with interrupts off), then the
 * stack pointer is swapped. Plus: the address space, the kernel stack
 * for the next interrupt from user mode, and TS - set, so that the first
 * fpu/sse instruction traps to math_state_restore(), unless the new task
 * still owns the fpu.
 */
.p2align 3
_switch_task:
	pushfq
	cli
	pushq %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	movq _current,%rax
	movq %rsp,ksp(%rax)
	movq ksp(%rdi),%rsp
	movq %rdi,_current
	movq cr3(%rdi),%rax
	movq %rax,%cr3
	leaq 8192(%rdi),%rax	# TASK_BLOCK: the top of its kernel stack
	movq %rax,_tss+4	# rsp0
	movq %cr0,%rax
	orq $8,%rax		# TS
	cmpq _last_task_used_math,%rdi
	jne 1f
	andq $~8,%rax
1:	movq %rax,%cr0
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	popfq
	ret

/*
 * A new task's first switch_task returns here, with %rsp at the copy
 * of its parent's registers that copy_process() made.
 */
.p2align 3
_ret_from_fork:
	jmp ret_from_sys_call

/*
 * ... or here for the init task, a kernel function in %rbx.
 */
.p2align 3
_kernel_thread_start:
	sti
	call *%rbx
	movq $-1,%rdi
	call _do_exit
