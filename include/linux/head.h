#ifndef _HEAD_H
#define _HEAD_H

/* 64-bit port: interrupt gates are 16 bytes, gdt entries still 8 */
typedef struct desc_struct {
	unsigned long a,b;
} desc_table[256];

extern desc_table idt;
extern unsigned long gdt[];
extern unsigned long tss_desc[2];
extern unsigned long pml4[512], pdpt_direct[512], pd0[512];

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_UCODE 3
#define GDT_UDATA 4
#define GDT_TSS 5

#endif
