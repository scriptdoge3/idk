/*
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 *
 * 64-bit port: do_page_fault() (memory.c) gets the saved registers, the
 * error code and the faulting address from %cr2, and decides between
 * do_no_page() and do_wp_page() as this code did.
 */

	.include "asm/regs.inc"

.globl _page_fault

_page_fault:
	pushq $0			# alignment (the cpu pushed an error code)
	SAVE_REGS
	movq %rsp,%rdi
	movq ERRCODE(%rsp),%rsi
	movq %cr2,%rdx
	call _do_page_fault
	RESTORE_REGS
	addq $16,%rsp
	iretq
