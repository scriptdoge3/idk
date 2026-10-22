/*
 * asm.s contains the low-level code for most hardware faults.
 * page_exception is handled by the mm, so that isn't here. This
 * file also handles (hopefully) fpu-exceptions due to TS-bit, as
 * the fpu must be properly saved/resored. This hasn't been tested.
 *
 * 64-bit port: every handler is called as do_xxx(struct regs *,
 * long error_code). The entry pushes the C function's address in the
 * frame's alignment slot, which is where the common code picks it up.
 */

	.include "asm/regs.inc"

.globl _divide_error,_debug,_nmi,_int3,_overflow,_bounds,_invalid_op
.globl _device_not_available,_double_fault,_coprocessor_segment_overrun
.globl _invalid_TSS,_segment_not_present,_stack_segment
.globl _general_protection,_coprocessor_error,_reserved,_simd_error

_divide_error:
	pushq $0			# "error code"
	pushq $_do_divide_error
error_code:
	SAVE_REGS
	movq PAD(%rsp),%rax		# the C function
	movq %rsp,%rdi
	movq ERRCODE(%rsp),%rsi
	call *%rax
	RESTORE_REGS
	addq $16,%rsp
	iretq

_debug:
	pushq $0
	pushq $_do_int3		# _do_debug
	jmp error_code

_nmi:
	pushq $0
	pushq $_do_nmi
	jmp error_code

_int3:
	pushq $0
	pushq $_do_int3
	jmp error_code

_overflow:
	pushq $0
	pushq $_do_overflow
	jmp error_code

_bounds:
	pushq $0
	pushq $_do_bounds
	jmp error_code

_invalid_op:
	pushq $0
	pushq $_do_invalid_op
	jmp error_code

/*
 * TS is set and the fpu (or sse) was touched: see math_state_restore()
 * in sched.c. There's no math emulation any more - every x86-64 cpu
 * has the fpu.
 */
_device_not_available:
	pushq $0
	pushq $_math_state_restore
	jmp error_code

_coprocessor_segment_overrun:
	pushq $0
	pushq $_do_coprocessor_segment_overrun
	jmp error_code

_reserved:
	pushq $0
	pushq $_do_reserved
	jmp error_code

_coprocessor_error:
	pushq $0
	pushq $_do_coprocessor_error
	jmp error_code

_simd_error:
	pushq $0
	pushq $_do_coprocessor_error
	jmp error_code

/* these come with an error code from the cpu */
_double_fault:
	pushq $_do_double_fault
	jmp error_code

_invalid_TSS:
	pushq $_do_invalid_TSS
	jmp error_code

_segment_not_present:
	pushq $_do_segment_not_present
	jmp error_code

_stack_segment:
	pushq $_do_stack_segment
	jmp error_code

_general_protection:
	pushq $_do_general_protection
	jmp error_code
