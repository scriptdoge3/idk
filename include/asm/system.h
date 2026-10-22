/*
 * 64-bit port: move_to_user_mode() is gone - task 0 stays in the kernel
 * as the idle task, and init enters user mode through execve().
 */

#define sti() __asm__ volatile ("sti":::"memory")
#define cli() __asm__ volatile ("cli":::"memory")
#define nop() __asm__ volatile ("nop")
#define save_flags(x) __asm__ volatile ("pushfq ; popq %0":"=r" (x)::"memory")
#define restore_flags(x) __asm__ volatile ("pushq %0 ; popfq"::"r" (x):"memory")

/*
 * A 64-bit gate: offset 0-15 | selector | ist, type, dpl, present |
 * offset 16-31, then offset 32-63 in the second quadword.
 */
#define _set_gate(gate_addr,type,dpl,ist,addr) do { \
	unsigned long __a = (unsigned long) (addr); \
	(gate_addr)->a = (__a & 0xffff) | (0x08UL << 16) | \
		((unsigned long) (ist) << 32) | \
		((unsigned long) (0x8000+((dpl)<<13)+((type)<<8)) << 32) | \
		((__a & 0xffff0000UL) << 32); \
	(gate_addr)->b = __a >> 32; \
} while (0)

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,0,addr)

#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,0,addr)

/* an interrupt gate that switches to stack 'ist' of the tss */
#define set_ist_gate(n,addr,ist) \
	_set_gate(&idt[n],14,0,ist,addr)
