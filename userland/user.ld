/*
 * Linux 0.01 runs every process in its own address space starting at
 * virtual address 0, and exec reads the text+data straight there from
 * file offset 1024 (a ZMAGIC a.out). So: everything linked at 0, one
 * contiguous blob, bss last.
 */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
	. = 0;
	.text : { *(.text .text.*) }
	.rodata : { *(.rodata .rodata.*) }
	.data : { *(.data .data.*) }
	.bss : { *(.bss .bss.*) *(COMMON) }
	end = .;

	/DISCARD/ : { *(.note .note.*) *(.comment) *(.eh_frame*) }
}
