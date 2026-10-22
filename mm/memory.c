/*
 *  memory.c
 *
 * 64-bit port. The 1991 version kept one page directory at address 0
 * and gave each task a 64Mb slice of it; that design needs segment
 * bases, which long mode doesn't have. Now:
 *
 * - every task has its own 4-level page tables (task->pml4). The upper
 *   half (the kernel) is shared, copied from the boot tables in head.s;
 *   the lower half, 0 .. TASK_SIZE, is the process.
 * - all physical memory is mapped at PAGE_OFFSET, so get_free_page() can
 *   hand out pointers, as before, and memory over 4Gb works like any.
 * - mem_map has a reference count per page, as before, but its size and
 *   place depend on the memory the BIOS reports.
 *
 * The functions are the 1991 ones, minus the 4Mb alignment rules.
 */
#include <signal.h>

#include <linux/config.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

int do_exit(long code);
void die(char * str,struct regs * regs,long nr);

#define invalidate() \
__asm__ volatile ("movq %%rax,%%cr3"::"a" (current->cr3):"memory")

/* present, writable, user. Page-table entries hold physical addresses */
#define PAGE_FLAGS 7
#define PT_ADDR(e) ((e) & 0x000ffffffffff000UL)
#define TABLE(e) ((unsigned long *) VIRT(PT_ADDR(e)))
#define USER_PML4_ENTRIES (TASK_SIZE >> 39)

unsigned long buffer_end;		/* physical end of the buffer cache */
static unsigned long low_mem;		/* first physical page that is paged */
static unsigned long high_memory;	/* end of physical memory */
static unsigned long paging_pages;
static unsigned short * mem_map;	/* one count per page from low_mem */
/*
 * 64-bit port: every page over free_hint is in use. 1991 scanned all of
 * mem_map from the top for each page - fine for 1500 pages, but with a
 * million and more, allocating a big block took quadratic time.
 */
static long free_hint;

#define MAP_NR(addr) (((addr)-low_mem)>>12)
#define USED 100

static void copy_page(unsigned long from,unsigned long to)
{
	unsigned long * f = (unsigned long *) from, * t = (unsigned long *) to;
	int i;

	for (i=0 ; i<PAGE_SIZE/8 ; i++)
		t[i] = f[i];
}

static void clear_page(unsigned long page)
{
	unsigned long * p = (unsigned long *) page;
	int i;

	for (i=0 ; i<PAGE_SIZE/8 ; i++)
		p[i] = 0;
}

/*
 * Get address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0. The address is a kernel
 * pointer (PAGE_OFFSET + physical).
 */
unsigned long get_free_page(void)
{
	long i;

	for (i=free_hint ; i>=0 ; i--)
		if (!mem_map[i]) {
			mem_map[i] = 1;
			free_hint = i-1;
			clear_page(VIRT(low_mem + (i<<12)));
			return VIRT(low_mem + (i<<12));
		}
	return 0;
}

/* two adjacent pages, for a task_struct and its kernel stack */
unsigned long get_free_pages2(void)
{
	long i;

	for (i=free_hint ; i>0 ; i--)
		if (!mem_map[i] && !mem_map[i-1]) {
			mem_map[i] = mem_map[i-1] = 1;
			if (i == free_hint)
				free_hint = i-2;
			clear_page(VIRT(low_mem + ((i-1)<<12)));
			clear_page(VIRT(low_mem + (i<<12)));
			return VIRT(low_mem + ((i-1)<<12));
		}
	return 0;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
static void free_phys_page(unsigned long addr)
{
	if (addr<low_mem) return;
	if (addr>=high_memory)
		panic("trying to free nonexistent page");
	addr = MAP_NR(addr);
	if (mem_map[addr]--) {
		if (!mem_map[addr] && addr > free_hint)
			free_hint = addr;
		return;
	}
	mem_map[addr]=0;
	panic("trying to free free page");
}

/* ... and the same for what get_free_page() returned */
void free_page(unsigned long addr)
{
	free_phys_page(PHYS(addr));
}

/*
 * The page-table entry for 'address' in 'pml4', making the missing
 * tables on the way if 'create'. NULL if there isn't one (or no memory).
 */
static unsigned long * get_pte(unsigned long * pml4,unsigned long address,
	int create)
{
	unsigned long * table = pml4, * e, tmp;
	int shift;

	for (shift=39 ; shift>12 ; shift-=9) {
		e = table + ((address >> shift) & 511);
		if (!(1 & *e)) {
			if (!create || !(tmp=get_free_page()))
				return NULL;
			*e = PHYS(tmp) | PAGE_FLAGS;
		}
		table = TABLE(*e);
	}
	return table + ((address >> 12) & 511);
}

/* a new pml4, sharing the kernel's half */
unsigned long * new_pml4(void)
{
	unsigned long * p;
	int i;

	if (!(p = (unsigned long *) get_free_page()))
		return NULL;
	for (i=256 ; i<512 ; i++)
		p[i] = pml4[i];
	return p;
}

static void free_table(unsigned long * table,int level)
{
	int i;

	for (i=0 ; i<512 ; i++) {
		if (!(1 & table[i]))
			continue;
		if (level > 1)
			free_table(TABLE(table[i]),level-1);
		free_phys_page(PT_ADDR(table[i]));
		table[i] = 0;
	}
}

/*
 * This function frees the user half of an address space, as needed
 * by 'exit()' and 'exec()'. The pml4 page itself stays.
 */
void free_page_tables(unsigned long * dir)
{
	int i;

	if (dir == pml4)
		panic("Trying to free up swapper memory space");
	for (i=0 ; i<USER_PML4_ENTRIES ; i++) {
		if (!(1 & dir[i]))
			continue;
		free_table(TABLE(dir[i]),3);
		free_phys_page(PT_ADDR(dir[i]));
		dir[i] = 0;
	}
	invalidate();
}

static int copy_table(unsigned long * from,unsigned long * to,int level,
	int nr)
{
	unsigned long this_page, tmp;
	int i;

	for (i=0 ; i<nr ; i++) {
		this_page = from[i];
		if (!(1 & this_page))
			continue;
		if (level == 1) {
			this_page &= ~2UL;
			from[i] = to[i] = this_page;
			mem_map[MAP_NR(PT_ADDR(this_page))]++;
			continue;
		}
		if (!(tmp = get_free_page()))
			return -1;	/* Out of memory, see freeing */
		to[i] = PHYS(tmp) | PAGE_FLAGS;
		if (copy_table(TABLE(this_page),(unsigned long *) tmp,
		    level-1,512))
			return -1;
	}
	return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * 64-bit port: it copies the whole user half of one address space into
 * another, for fork(). Both end up sharing every page read-only, with
 * the counts in mem_map going up: copy-on-write, as in 1991.
 */
int copy_page_tables(unsigned long * from,unsigned long * to)
{
	int ret = copy_table(from,to,4,USER_PML4_ENTRIES);

	invalidate();
	return ret;
}

/*
 * This function puts a page in memory at the wanted address of the
 * current process. It returns the page, 0 if out of memory (either when
 * trying to access page-table or page.)
 */
unsigned long put_page(unsigned long page,unsigned long address)
{
	unsigned long * pte;

	if (PHYS(page) < low_mem || PHYS(page) >= high_memory)
		printk("Trying to put page %p at %p\n",page,address);
	if (mem_map[MAP_NR(PHYS(page))] != 1)
		printk("mem_map disagrees with %p at %p\n",page,address);
	if (!(pte = get_pte(current->pml4,address,1)))
		return 0;
	*pte = PHYS(page) | PAGE_FLAGS;
	return page;
}

void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page,new_page;

	old_page = PT_ADDR(*table_entry);
	if (old_page >= low_mem && mem_map[MAP_NR(old_page)]==1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	if (!(new_page=get_free_page()))
		do_exit(SIGSEGV);
	if (old_page >= low_mem)
		mem_map[MAP_NR(old_page)]--;
	copy_page(VIRT(old_page),new_page);
	*table_entry = PHYS(new_page) | PAGE_FLAGS;
	invalidate();
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
	unsigned long * pte = get_pte(current->pml4,address,0);

	if (!pte || !(1 & *pte))
		do_exit(SIGSEGV);
	un_wp_page(pte);
}

void write_verify(unsigned long address)
{
	unsigned long * pte;

	if (address >= TASK_SIZE)
		return;
	if (!(pte = get_pte(current->pml4,address,0)))
		return;
	if ((3 & *pte) == 1)  /* non-writeable, present */
		un_wp_page(pte);
	return;
}

void do_no_page(unsigned long error_code,unsigned long address)
{
	unsigned long tmp;

	if (tmp=get_free_page())
		if (put_page(tmp,address & ~0xfffUL))
			return;
	do_exit(SIGSEGV);
}

/*
 * Called from page.s. 1991 had only the two cases below; a process
 * reaching above TASK_SIZE (into the kernel) is new, and is killed.
 */
void do_page_fault(struct regs * regs,unsigned long error_code,
	unsigned long address)
{
	if (address >= TASK_SIZE) {
		if (regs->cs & 3)
			do_exit(SIGSEGV);
		printk("Unable to handle kernel paging request at %p\n\r",
			address);
		die("page fault",regs,error_code);
	}
	if (error_code & 1)
		do_wp_page(error_code,address);
	else
		do_no_page(error_code,address);
}

/*
 * The BIOS memory map, as boot.s left it: a count at 0x90600 and 20-byte
 * entries from 0x90608. Type 1 is usable ram.
 */
struct e820 {
	unsigned long base;
	unsigned long length;
	unsigned int type;
} __attribute__((packed));

#define E820_MAX 32
static struct e820 e820_map[E820_MAX];
static int e820_nr;

static unsigned int cmos(int addr)
{
	outb_p(0x80|addr,0x70);
	return inb_p(0x71);
}

/*
 * Called first thing by main(): find out how much memory there is, map
 * it all at PAGE_OFFSET and set up mem_map. Physical memory then is:
 *
 *	0 - end			the kernel
 *	end - 640kB, 1Mb - buffer_end	the buffer cache (fs/buffer.c)
 *	buffer_end - low_mem	page directories for the memory over 1Gb
 *				(pd0 in head.s covers the first), mem_map
 *	low_mem - high_memory	pages, handed out by get_free_page()
 */
void mem_init(void)
{
	struct e820 * e;
	unsigned long p, start, end, gb, nr;
	int i;

	current->cr3 = PHYS(pml4);	/* task 0 runs on the boot tables */
	e820_nr = *(unsigned short *) LOW(0x90600);
	if (e820_nr > E820_MAX)
		e820_nr = E820_MAX;
	for (i=0 ; i<e820_nr ; i++)
		e820_map[i] = ((struct e820 *) LOW(0x90608))[i];
	if (!e820_nr) {		/* old bios: the cmos knows up to 64Mb */
		e820_map[0].base = 0;
		e820_map[0].length = 0xa0000;
		e820_map[0].type = 1;
		e820_map[1].base = 0x100000;
		e820_map[1].length = (cmos(0x30) | (cmos(0x31)<<8)) * 1024UL;
		e820_map[1].type = 1;
		e820_nr = 2;
	}
	high_memory = 0;
	for (i=0,e=e820_map ; i<e820_nr ; i++,e++)
		if (e->type == 1 && e->base + e->length > high_memory)
			high_memory = e->base + e->length;
	if (high_memory > MAX_MEMORY)
		high_memory = MAX_MEMORY;
	high_memory &= ~0xfffUL;
	if (high_memory < 0x800000)
		panic("Need at least 8Mb of memory");
	buffer_end = (high_memory >= 0x2000000) ? 0x400000 : 0x200000;
/* map everything over the first Gb, 2Mb pages again */
	p = buffer_end;
	for (gb=1 ; gb < (high_memory + (1UL<<30) - 1) >> 30 ; gb++) {
		unsigned long * pd = (unsigned long *) LOW(p);
		for (i=0 ; i<512 ; i++)
			pd[i] = (gb<<30) + ((unsigned long) i<<21) + 0x83;
		pdpt_direct[gb] = p + 3;
		p += PAGE_SIZE;
	}
	invalidate();
/* mem_map, and what's left is paged */
	mem_map = (unsigned short *) LOW(p);
	nr = (high_memory - p) >> 12;
	low_mem = PAGE_ALIGN(p + 2*nr);
	paging_pages = (high_memory - low_mem) >> 12;
	free_hint = paging_pages-1;
	for (i=0 ; i<paging_pages ; i++)
		mem_map[i] = USED;
	for (i=0,e=e820_map ; i<e820_nr ; i++,e++) {
		if (e->type != 1)
			continue;
		start = PAGE_ALIGN(e->base);
		end = (e->base + e->length) & ~0xfffUL;
		if (start < low_mem)
			start = low_mem;
		if (end > high_memory)
			end = high_memory;
		for ( ; start < end ; start += PAGE_SIZE)
			mem_map[MAP_NR(start)] = 0;
	}
}

static long nr_free_pages(void)
{
	long i, free = 0;

	for (i=0 ; i<paging_pages ; i++)
		if (!mem_map[i])
			free++;
	return free;
}

/* pages that exist: paging_pages less the holes in the bios map */
static long nr_ram_pages(void)
{
	long i, nr = 0;

	for (i=0 ; i<paging_pages ; i++)
		if (mem_map[i] != USED)
			nr++;
	return nr;
}

void calc_mem(void)
{
	int i;
	unsigned long ram = 0;

	for (i=0 ; i<e820_nr ; i++)
		if (e820_map[i].type == 1)
			ram += e820_map[i].length;
	printk("Memory: %dMb of ram, physical addresses up to %p\n\r",
		(int) (ram >> 20),high_memory);
	for (i=0 ; i<e820_nr ; i++)
		printk("  %p - %p %s\n\r",e820_map[i].base,
			e820_map[i].base+e820_map[i].length,
			e820_map[i].type == 1 ? "ram" : "reserved");
	printk("%d pages free (of %d)\n\r",(int) nr_free_pages(),
		(int) nr_ram_pages());
}

/*
 * 64-bit port: so that a process can see the memory (/bin/free).
 * Fills in total and free bytes of paged memory, and the end of
 * physical memory.
 */
long sys_meminfo(unsigned long * buf)
{
	verify_area(buf,3*sizeof(unsigned long));
	put_fs_long(nr_ram_pages() << 12,buf);
	put_fs_long(nr_free_pages() << 12,buf+1);
	put_fs_long(high_memory,buf+2);
	return 0;
}
