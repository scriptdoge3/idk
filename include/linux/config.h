#ifndef _CONFIG_H
#define _CONFIG_H

/* #define LASU_HD */
/* #define LINUS_HD */

/*
 * Modern port: a single emulated IDE disk (see the Makefile, which
 * builds hd.img and tells QEMU this exact geometry). The root file
 * system lives in its first partition.
 */
#define QEMU_HD

/*
 * 1991: "Amount of ram memory (in bytes, 640k-1M not discounted).
 * Currently 8Mb." and a fixed BUFFER_END.
 *
 * 64-bit port: the memory size comes from the BIOS (boot.s asks for the
 * e820 map) and mm/memory.c picks the buffer cache size. MAX_MEMORY
 * caps what is used: mem_map needs 2 bytes per 4kB page, so 64Gb costs
 * 32Mb of it. Must be a multiple of 1Gb, at most 512Gb.
 */
#define MAX_MEMORY	(64UL<<30)

/* Root device at bootup. */
#if	defined(QEMU_HD)
#define ROOT_DEV 0x301
#elif	defined(LINUS_HD)
#define ROOT_DEV 0x306
#elif	defined(LASU_HD)
#define ROOT_DEV 0x302
#else
#error "must define HD"
#endif

/*
 * HD type. If 2, put 2 structures with a comma. If just 1, put
 * only 1 struct. The structs are { HEAD, SECTOR, TRACKS, WPCOM, LZONE, CTL }
 *
 * NOTE. CTL is supposed to be 0 for drives with less than 8 heads, and
 * 8 if heads >= 8. Don't know why, and I haven't tested it on a drive with
 * more than 8 heads, but that is what the bios-listings seem to imply. I
 * just love not having a manual.
 */
#if	defined(QEMU_HD)
#define HD_TYPE { 16,63,64,65535,64,8 }
#elif	defined(LASU_HD)
#define HD_TYPE { 7,35,915,65536,920,0 }
#elif	defined(LINUS_HD)
#define HD_TYPE { 5,17,980,300,980,0 },{ 5,17,980,300,980,0 }
#else
#error "must define a hard-disk type"
#endif

#endif
