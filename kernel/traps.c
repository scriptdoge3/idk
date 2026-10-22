/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include <string.h>

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>
#include <asm/segment.h>

int do_exit(long code);

void page_exception(void);

void divide_error(void);
void debug(void);
void nmi(void);
void int3(void);
void overflow(void);
void bounds(void);
void invalid_op(void);
void device_not_available(void);
void double_fault(void);
void coprocessor_segment_overrun(void);
void invalid_TSS(void);
void segment_not_present(void);
void stack_segment(void);
void general_protection(void);
void page_fault(void);
void coprocessor_error(void);
void simd_error(void);
void reserved(void);

/*
 * 64-bit port: the registers come as a struct regs (asm.s). A fault in
 * user mode still costs the process a SIGSEGV after the register dump,
 * as in 1991. One in the kernel is a kernel bug: dump and stop.
 */
void die(char * str,struct regs * regs,long nr)
{
	int i;

	printk("%s: %04x\n\r",str,(int) nr&0xffff);
	printk("RIP: %04x:%p  RFLAGS: %p  RSP: %04x:%p\n\r",
		(int) regs->cs,regs->rip,regs->rflags,(int) regs->ss,regs->rsp);
	printk("RAX: %p RBX: %p RCX: %p RDX: %p\n\r",
		regs->rax,regs->rbx,regs->rcx,regs->rdx);
	printk("RSI: %p RDI: %p RBP: %p\n\r",regs->rsi,regs->rdi,regs->rbp);
	printk("Pid: %d, cr3: %p\n\r",(int) current->pid,current->cr3);
	if (regs->rip < TASK_SIZE || regs->rip >= KERNEL_BASE) {
		for(i=0;i<10;i++)
			printk("%02x ",0xff & ((regs->cs & 3) ?
				get_fs_byte(i+(char *)regs->rip) :
				*(i+(char *)regs->rip)));
		printk("\n\r");
	}
	if (!(regs->cs & 3))
		panic("kernel fault");
	do_exit(11);		/* play segment exception */
}

void do_double_fault(struct regs * regs, long error_code)
{
	die("double fault",regs,error_code);
}

void do_general_protection(struct regs * regs, long error_code)
{
	die("general protection",regs,error_code);
}

void do_divide_error(struct regs * regs, long error_code)
{
	die("divide error",regs,error_code);
}

void do_int3(struct regs * regs, long error_code)
{
	printk("rax\t\trbx\t\trcx\t\trdx\n\r%p\t%p\t%p\t%p\n\r",
		regs->rax,regs->rbx,regs->rcx,regs->rdx);
	printk("rsi\t\trdi\t\trbp\t\trsp\n\r%p\t%p\t%p\t%p\n\r",
		regs->rsi,regs->rdi,regs->rbp,regs->rsp);
	printk("RIP: %p   CS: %4x  RFLAGS: %p\n\r",regs->rip,
		(int) regs->cs,regs->rflags);
}

void do_nmi(struct regs * regs, long error_code)
{
	die("nmi",regs,error_code);
}

void do_debug(struct regs * regs, long error_code)
{
	die("debug",regs,error_code);
}

void do_overflow(struct regs * regs, long error_code)
{
	die("overflow",regs,error_code);
}

void do_bounds(struct regs * regs, long error_code)
{
	die("bounds",regs,error_code);
}

void do_invalid_op(struct regs * regs, long error_code)
{
	die("invalid operand",regs,error_code);
}

void do_coprocessor_segment_overrun(struct regs * regs, long error_code)
{
	die("coprocessor segment overrun",regs,error_code);
}

void do_invalid_TSS(struct regs * regs,long error_code)
{
	die("invalid TSS",regs,error_code);
}

void do_segment_not_present(struct regs * regs,long error_code)
{
	die("segment not present",regs,error_code);
}

void do_stack_segment(struct regs * regs,long error_code)
{
	die("stack segment",regs,error_code);
}

void do_coprocessor_error(struct regs * regs, long error_code)
{
	die("coprocessor error",regs,error_code);
}

void do_reserved(struct regs * regs, long error_code)
{
	die("reserved (15,17-31) error",regs,error_code);
}

void trap_init(void)
{
	int i;

	set_trap_gate(0,&divide_error);
	set_trap_gate(1,&debug);
	set_trap_gate(2,&nmi);
	set_system_gate(3,&int3);	/* int3-5 can be called from all */
	set_system_gate(4,&overflow);
	set_system_gate(5,&bounds);
	set_trap_gate(6,&invalid_op);
	set_trap_gate(7,&device_not_available);
	set_ist_gate(8,&double_fault,1);	/* on a stack of its own */
	set_trap_gate(9,&coprocessor_segment_overrun);
	set_trap_gate(10,&invalid_TSS);
	set_trap_gate(11,&segment_not_present);
	set_trap_gate(12,&stack_segment);
	set_trap_gate(13,&general_protection);
	set_trap_gate(14,&page_fault);
	set_trap_gate(15,&reserved);
	set_trap_gate(16,&coprocessor_error);
	for (i=17;i<32;i++)
		set_trap_gate(i,&reserved);
	set_trap_gate(19,&simd_error);
/*	__asm__("movl $0x3ff000,%%eax\n\t"
		"movl %%eax,%%db0\n\t"
		"movl $0x000d0303,%%eax\n\t"
		"movl %%eax,%%db7"
		:::"ax");*/
}

