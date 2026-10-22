/*
 * Linker script for the kernel ("tools/system").
 *
 * 64-bit port: the kernel runs at KERNEL_BASE (the top 2Gb of the
 * address space, see boot/head.s) but is loaded at physical 0, which is
 * what the AT()s say; objcopy lays the flat image out by those load
 * addresses. head.s must come first: the boot loader jumps to 0.
 *
 * .bss is folded into .data so that the flat binary carries it as
 * zeroes - the original a.out kernel relied on gcc 1.x putting
 * zero-initialised tables in .data.
 */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(startup_32)

KERNEL_BASE = 0xffffffff80000000;

SECTIONS
{
	. = KERNEL_BASE;
	.text : AT(ADDR(.text) - KERNEL_BASE) {
		*(.text .text.*)
	}
	_etext = .;
	.rodata : AT(ADDR(.rodata) - KERNEL_BASE) {
		*(.rodata .rodata.*)
	}
	.data : AT(ADDR(.data) - KERNEL_BASE) {
		*(.data .data.*)
		_edata = .;
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(8);
	}
	_end = .;

	/DISCARD/ : {
		*(.note .note.*)
		*(.comment)
		*(.eh_frame)
	}
}
