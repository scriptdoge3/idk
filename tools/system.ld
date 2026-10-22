/*
 * Linker script for the kernel ("tools/system").
 *
 * head.s must come first: it starts at absolute address 0, which is also
 * where the page directory ends up. .bss is folded into .data so that
 * the flat binary carries it as zeroes - the original a.out kernel relied
 * on gcc 1.x putting zero-initialised tables in .data.
 */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(startup_32)

SECTIONS
{
	. = 0;
	.text : {
		*(.text .text.*)
	}
	_etext = .;
	.rodata : {
		*(.rodata .rodata.*)
	}
	.data : {
		*(.data .data.*)
		_edata = .;
		*(.bss .bss.*)
		*(COMMON)
		. = ALIGN(4);
	}
	_end = .;

	/DISCARD/ : {
		*(.note .note.*)
		*(.comment)
		*(.eh_frame)
	}
}
