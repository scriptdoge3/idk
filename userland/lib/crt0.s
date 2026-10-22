/*
 * crt0.s - process entry point for Linux 0.01 a.out binaries.
 *
 * execve() (fs/exec.c, create_tables) leaves the stack as
 *	argc, argv, envp
 * (64-bit words now), which main(argc, argv, envp) gets in %rdi, %rsi,
 * %rdx in the 64-bit calling convention, on a 16-byte aligned stack.
 */
	.text
	.globl	_start
_start:
	movq	(%rsp),%rdi
	movq	8(%rsp),%rsi
	movq	16(%rsp),%rdx
	movq	%rdx,environ
	andq	$-16,%rsp
	call	main
	movl	%eax,%edi
	call	exit
1:	jmp	1b

	.section .note.GNU-stack,"",@progbits
