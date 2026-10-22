/*
 *  head.s contains the startup code.
 *
 * The boot loader (boot.s) jumps here, to absolute address 0, in 32-bit
 * protected mode with paging off. The 1991 kernel just set up paging
 * for the first 8Mb and called main().
 *
 * 64-bit port: from here we go on into long mode. That needs paging
 * (4 levels, PAE format), so a set of static page tables below maps the
 * first 1Gb of physical memory three times:
 *
 *	0x0000000000000000	identity, only while we switch modes
 *	0xffff800000000000	PAGE_OFFSET: all physical memory (mm/memory.c
 *				extends this past 1Gb once it knows the size)
 *	0xffffffff80000000	KERNEL_BASE: the kernel image, linked here
 *
 * Everything in this file before the jump to the high addresses runs at
 * physical addresses, hence all the "- KERNEL_BASE".
 */

KERNEL_BASE	= 0xffffffff80000000

.text
.globl startup_32,_idt,_gdt,_pml4,_pdpt_direct,_pd0,_tss_desc

	.code32
startup_32:
	movl $0x10,%eax
	mov %ax,%ds
	mov %ax,%es
	mov %ax,%fs
	mov %ax,%gs
	mov %ax,%ss
	movl $(boot_stack - KERNEL_BASE),%esp
	xorl %eax,%eax
1:	incl %eax		# check that A20 really IS enabled
	movl %eax,0x000500
	cmpl %eax,0x100500
	je 1b

/* no long mode, no 64-bit kernel: say so and stop */
	movl $0x80000000,%eax
	cpuid
	cmpl $0x80000001,%eax
	jb no_long_mode
	movl $0x80000001,%eax
	cpuid
	btl $29,%edx		# LM
	jnc no_long_mode

	movl %cr4,%eax
	orl $0x620,%eax		# PAE, and OSFXSR/OSXMMEXCPT: we save the
	movl %eax,%cr4		# sse registers too (fxsave) - turns sse on
	movl $(_pml4 - KERNEL_BASE),%eax
	movl %eax,%cr3
	movl $0xc0000080,%ecx	# EFER
	rdmsr
	orl $0x100,%eax		# LME
	wrmsr
	movl %cr0,%eax
	andl $~0x0c,%eax	# EM, TS off: the fpu/sse unit is used
	orl $0x80000003,%eax	# PG, MP, PE. WP stays off, as on a 386:
	movl %eax,%cr0		# verify_area() handles copy-on-write
	lgdt (gdt_desc32 - KERNEL_BASE)
	ljmp $0x08,$(startup_64 - KERNEL_BASE)

no_long_mode:
	movl $(no_lm_msg - KERNEL_BASE),%esi
	movl $0xb8000,%edi
	movb $0x0c,%ah		# light red
1:	lodsb
	testb %al,%al
	je 2f
	stosw
	jmp 1b
2:	hlt
	jmp 2b

no_lm_msg:
	.asciz "This kernel needs a 64-bit (x86-64) CPU."

	.code64
startup_64:
	movabsq $1f,%rax	# on to the kernel's real addresses
	jmp *%rax
1:	lgdt gdt_desc64
	movl $0x10,%eax		# reload all the segment registers
	mov %ax,%ds		# after changing gdt.
	mov %ax,%es
	mov %ax,%ss
	xorl %eax,%eax
	mov %ax,%fs
	mov %ax,%gs
	movq _stack_start,%rsp
	call setup_idt
	cld
	xorl %edi,%edi		# These are the parameters to main :-)
	xorl %esi,%esi
	xorl %edx,%edx
	call _main
L6:	hlt			# main should never return here, but
	jmp L6			# just in case, we know what happens.

/*
 *  setup_idt
 *
 *  sets up a idt with 256 entries pointing to
 *  ignore_int, interrupt gates. It then loads
 *  idt. Everything that wants to install itself
 *  in the idt-table may do so themselves. Interrupts
 *  are enabled elsewhere, when we can be relatively
 *  sure everything is ok.
 *
 *  A 64-bit gate is 16 bytes: offset 0-15, selector, type (0x8E is a
 *  present interrupt gate, dpl=0), offset 16-31, offset 32-63, zero.
 */
setup_idt:
	leaq ignore_int,%rdx
	movl $0x00080000,%eax
	movw %dx,%ax		/* selector = 0x0008 = cs */
	movw $0x8E00,%dx	/* interrupt gate - dpl=0, present */
	movq %rdx,%rcx
	shrq $32,%rcx		/* offset 32-63 */
	leaq _idt,%rdi
	movl $256,%esi
rp_sidt:
	movl %eax,(%rdi)
	movl %edx,4(%rdi)
	movq %rcx,8(%rdi)
	addq $16,%rdi
	decl %esi
	jne rp_sidt
	lidt idt_descr
	ret

/* This is the default interrupt "handler" :-) */
.p2align 3
ignore_int:
	incb KERNEL_BASE+0xb8000+160	# put something on the screen
	movb $2,KERNEL_BASE+0xb8000+161	# so that we know something
	iretq				# happened

.p2align 3
.word 0,0,0
idt_descr:
	.word 256*16-1		# idt contains 256 entries
	.quad _idt
.p2align 3
.word 0,0,0
gdt_desc64:
	.word gdt_end-_gdt-1
	.quad _gdt
.p2align 3
.word 0,0,0
gdt_desc32:
	.word gdt_end-_gdt-1
	.long _gdt - KERNEL_BASE

/*
 * The selectors are fixed: 0x08 kernel code, 0x10 kernel data, 0x1b user
 * code, 0x23 user data (see USER_CS/USER_DS in <linux/sched.h>), 0x28 the
 * one TSS. 1991's per-task TSS and LDT entries are gone: long mode has
 * neither hardware task switching nor segment bases to separate tasks.
 */
.p2align 3
_gdt:	.quad 0x0000000000000000	/* NULL descriptor */
	.quad 0x00af9a000000ffff	/* kernel code, 64-bit */
	.quad 0x00cf92000000ffff	/* kernel data */
	.quad 0x00affa000000ffff	/* user code, 64-bit, dpl 3 */
	.quad 0x00cff2000000ffff	/* user data, dpl 3 */
_tss_desc:
	.quad 0,0			/* tss (16 bytes), see sched_init() */
gdt_end:

.p2align 4
_idt:	.fill 512,8,0			# idt is uninitialized (256 16-byte gates)

	.fill 256,8,0			# just enough stack to get to main()
boot_stack:

/*
 * The page tables. Entries are physical addresses + 3 (present, r/w).
 * pd0 maps the first 1Gb with 2Mb pages (+0x80).
 */
.p2align 12
_pml4:
	.quad pdpt_low - KERNEL_BASE + 3	/* removed by main() */
	.fill 255,8,0
	.quad _pdpt_direct - KERNEL_BASE + 3	/* PAGE_OFFSET */
	.fill 254,8,0
	.quad pdpt_kernel - KERNEL_BASE + 3	/* KERNEL_BASE */
pdpt_low:
	.quad _pd0 - KERNEL_BASE + 3
	.fill 511,8,0
_pdpt_direct:
	.quad _pd0 - KERNEL_BASE + 3
	.fill 511,8,0
pdpt_kernel:
	.fill 510,8,0
	.quad _pd0 - KERNEL_BASE + 3
	.quad 0
_pd0:
	.set i,0
	.rept 512
	.quad (i << 21) + 0x83
	.set i,i+1
	.endr
