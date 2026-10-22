/*
 * regs.inc - the register frame of the low-level entry code, for the
 * assembly files (.include "asm/regs.inc"). It must match struct regs
 * in <linux/sched.h>.
 *
 * On any interrupt the cpu (in long mode, always) pushes ss, rsp,
 * rflags, cs, rip and for some exceptions an error code. The entry code
 * pushes a 0 error code where the cpu didn't, then one more word to keep
 * the C stack 16-byte aligned (the asm uses it to pass the C handler),
 * then SAVE_REGS. %rsp then points at a struct regs.
 */

R15	= 0x00
R14	= 0x08
R13	= 0x10
R12	= 0x18
R11	= 0x20
R10	= 0x28
R9	= 0x30
R8	= 0x38
RBP	= 0x40
RDI	= 0x48
RSI	= 0x50
RDX	= 0x58
RCX	= 0x60
RBX	= 0x68
RAX	= 0x70
PAD	= 0x78
ERRCODE	= 0x80
RIP	= 0x88
CS	= 0x90
RFLAGS	= 0x98
OLDRSP	= 0xa0
OLDSS	= 0xa8

/* offsets into the task-struct, see <linux/sched.h> */
state	= 0
counter	= 8
priority = 16
signal	= 24
ksp	= 32
cr3	= 40

.macro SAVE_REGS
	pushq %rax
	pushq %rbx
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %rbp
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	cld			/* gcc-compiled code expects DF=0 */
.endm

.macro RESTORE_REGS
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rbp
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rbx
	popq %rax
.endm
