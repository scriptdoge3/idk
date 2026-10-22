/*
 *	rs_io.s
 *
 * This module implements the rs232 io interrupts.
 *
 * 64-bit port: pointers are 8 bytes, so a table_list pair is 16 and the
 * tty_queue offsets doubled. The queue pointer pair for the interrupting
 * line travels in %r12 instead of on the stack.
 */

	.include "asm/regs.inc"

.text
.globl _rs1_interrupt,_rs2_interrupt

size	= 1024				/* must be power of two !
					   and must match the value
					   in tty_io.c!!! */

/* these are the offsets into the read/write buffer structures */
rs_addr = 0
head = 8
tail = 16
proc_list = 24
buf = 32

startup	= 256		/* chars left in write queue when we restart it */

/*
 * These are the actual interrupt routines. They look where
 * the interrupt is coming from, and take appropriate action.
 */
.p2align 3
_rs1_interrupt:
	pushq $0
	pushq $_table_list+16
	jmp rs_int
.p2align 3
_rs2_interrupt:
	pushq $0
	pushq $_table_list+32
rs_int:
	SAVE_REGS
	movq PAD(%rsp),%r12		# &table_list[2*line]
	movq (%r12),%rdx
	movq rs_addr(%rdx),%rdx
	addl $2,%edx		/* interrupt ident. reg */
rep_int:
	xorl %eax,%eax
	inb %dx,%al
	testb $1,%al
	jne end
	cmpb $6,%al		/* this shouldn't happen, but ... */
	ja end
	movq %r12,%rcx
	pushq %rdx
	subl $2,%edx
	call *jmp_table(,%rax,4)		/* NOTE! not *8, bit0 is 0 already */
	popq %rdx
	jmp rep_int
end:	movb $0x20,%al
	outb %al,$0x20		/* EOI */
	RESTORE_REGS
	addq $16,%rsp
	iretq

jmp_table:
	.quad modem_status,write_char,read_char,line_status

.p2align 3
modem_status:
	addl $6,%edx		/* clear intr by reading modem status reg */
	inb %dx,%al
	ret

.p2align 3
line_status:
	addl $5,%edx		/* clear intr by reading line status reg. */
	inb %dx,%al
	ret

.p2align 3
read_char:
	inb %dx,%al
	movq %rcx,%rdx
	subq $_table_list,%rdx
	shrq $4,%rdx
	movq (%rcx),%rcx		# read-queue
	movq head(%rcx),%rbx
	movb %al,buf(%rcx,%rbx)
	incl %ebx
	andl $size-1,%ebx
	cmpq tail(%rcx),%rbx
	je 1f
	movq %rbx,head(%rcx)
	movq %rdx,%rdi
	call _do_tty_interrupt
1:	ret

.p2align 3
write_char:
	movq 8(%rcx),%rcx		# write-queue
	movq head(%rcx),%rbx
	subq tail(%rcx),%rbx
	andl $size-1,%ebx		# nr chars in queue
	je write_buffer_empty
	cmpl $startup,%ebx
	ja 1f
	movq proc_list(%rcx),%rbx	# wake up sleeping process
	testq %rbx,%rbx			# is there any?
	je 1f
	movq $0,(%rbx)
1:	movq tail(%rcx),%rbx
	movb buf(%rcx,%rbx),%al
	outb %al,%dx
	incl %ebx
	andl $size-1,%ebx
	movq %rbx,tail(%rcx)
	cmpq head(%rcx),%rbx
	je write_buffer_empty
	ret
.p2align 3
write_buffer_empty:
	movq proc_list(%rcx),%rbx	# wake up sleeping process
	testq %rbx,%rbx			# is there any?
	je 1f
	movq $0,(%rbx)
1:	incl %edx
	inb %dx,%al
	jmp 1f
1:	jmp 1f
1:	andb $0xd,%al		/* disable transmit interrupt */
	outb %al,%dx
	ret
