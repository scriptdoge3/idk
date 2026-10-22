/*
 * crt0.s - process entry point for Linux 0.01 a.out binaries.
 *
 * execve() (fs/exec.c, create_tables) leaves the stack as
 *	argc, argv, envp
 * which is exactly a cdecl call frame for main(argc, argv, envp)
 * minus the return address.
 */
	.text
	.globl	_start
_start:
	movl	8(%esp),%eax
	movl	%eax,environ
	call	main
	pushl	%eax
	call	exit
1:	jmp	1b

	.section .note.GNU-stack,"",@progbits
