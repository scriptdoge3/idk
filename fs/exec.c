#include <errno.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

#include <string.h>

extern long sys_exit(int exit_code);
extern long sys_close(int fd);

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 */
#define MAX_ARG_PAGES 32

/*
 * 64-bit port: the process is loaded at its own address 0 .. in the
 * current page tables, so a block is just copied there. Pages appear
 * as they are touched (do_no_page).
 */
#define cp_block(from,to) memcpy((void *) (unsigned long) (to),(from),BLOCK_SIZE)

/*
 * read_head() reads blocks 1-6 (not 0). Block 0 has already been
 * read for header information.
 */
int read_head(struct m_inode * inode,int blocks)
{
	struct buffer_head * bh;
	int count;

	if (blocks>6)
		blocks=6;
	for(count = 0 ; count<blocks ; count++) {
		if (!inode->i_zone[count+1])
			continue;
		if (!(bh=bread(inode->i_dev,inode->i_zone[count+1])))
			return -1;
		cp_block(bh->b_data,count*BLOCK_SIZE);
		brelse(bh);
	}
	return 0;
}

int read_ind(int dev,int ind,long size,unsigned long offset)
{
	struct buffer_head * ih, * bh;
	unsigned short * table,block;

	if (size<=0)
		panic("size<=0 in read_ind");
	if (size>512*BLOCK_SIZE)
		size=512*BLOCK_SIZE;
	if (!ind)
		return 0;
	if (!(ih=bread(dev,ind)))
		return -1;
	table = (unsigned short *) ih->b_data;
	while (size>0) {
		if (block=*(table++))
			if (!(bh=bread(dev,block))) {
				brelse(ih);
				return -1;
			} else {
				cp_block(bh->b_data,offset);
				brelse(bh);
			}
		size -= BLOCK_SIZE;
		offset += BLOCK_SIZE;
	}
	brelse(ih);
	return 0;
}

/*
 * read_area() reads an area into %fs:mem.
 */
int read_area(struct m_inode * inode,long size)
{
	struct buffer_head * dind;
	unsigned short * table;
	int i,count;

	if ((i=read_head(inode,(size+BLOCK_SIZE-1)/BLOCK_SIZE)) ||
	    (size -= BLOCK_SIZE*6)<=0)
		return i;
	if ((i=read_ind(inode->i_dev,inode->i_zone[7],size,BLOCK_SIZE*6)) ||
	    (size -= BLOCK_SIZE*512)<=0)
		return i;
	if (!(i=inode->i_zone[8]))
		return 0;
	if (!(dind = bread(inode->i_dev,i)))
		return -1;
	table = (unsigned short *) dind->b_data;
	for(count=0 ; count<512 ; count++)
		if ((i=read_ind(inode->i_dev,*(table++),size,	/* 1991: 518+count */
		    BLOCK_SIZE*(518+512*count))) || (size -= BLOCK_SIZE*512)<=0)
			return i;
	panic("Impossibly long executable");
}

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 */
static unsigned long * create_tables(char * p,int argc,int envc)
{
	unsigned long *argv,*envp;
	unsigned long * sp;

	sp = (unsigned long *) (~7UL & (unsigned long) p);
	sp -= envc+1;
	envp = sp;
	sp -= argc+1;
	argv = sp;
	put_fs_long((unsigned long)envp,--sp);
	put_fs_long((unsigned long)argv,--sp);
	put_fs_long((unsigned long)argc,--sp);
	while (argc-->0) {
		put_fs_long((unsigned long) p,argv++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0,argv);
	while (envc-->0) {
		put_fs_long((unsigned long) p,envp++);
		while (get_fs_byte(p++)) /* nothing */ ;
	}
	put_fs_long(0,envp);
	return sp;
}

/*
 * count() counts the number of arguments/envelopes
 */
static int count(char ** argv)
{
	int i=0;
	char ** tmp;

	if (tmp = argv)
		while (get_fs_long((unsigned long *) (tmp++)))
			i++;

	return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 */
static unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
		unsigned long p)
{
	int len,i;
	char *tmp;

	while (argc-- > 0) {
		if (!(tmp = (char *)get_fs_long(((unsigned long *) argv)+argc)))
			panic("argc is wrong");
		len=0;		/* remember zero-padding */
		do {
			len++;
		} while (get_fs_byte(tmp++));
		if (len > p)		/* this shouldn't happen - 128kB */
					/* (1991: p-len < 0, never true) */
			return 0;
		i = ((unsigned) (p-len)) >> 12;
		while (i<MAX_ARG_PAGES && !page[i]) {
			if (!(page[i]=get_free_page()))
				return 0;
			i++;
		}
		do {
			--p;
			if (!page[p/PAGE_SIZE])
				panic("nonexistent page in exec.c");
			((char *) page[p/PAGE_SIZE])[p%PAGE_SIZE] =
				get_fs_byte(--tmp);
		} while (--len);
	}
	return p;
}

/*
 * 1991's change_ldt() set the segments' bases and limits and put the
 * argument pages at the top of the 64Mb data segment. 64-bit port: no
 * segments; the argument pages go at the top of the address space.
 */
static unsigned long setup_arg_pages(unsigned long * page)
{
	unsigned long data_base;
	int i;

	data_base = TASK_SIZE;
	for (i=MAX_ARG_PAGES-1 ; i>=0 ; i--) {
		data_base -= PAGE_SIZE;
		if (page[i])
			put_page(page[i],data_base);
	}
	return TASK_SIZE;
}

/*
 * 'do_execve()' executes a new program.
 *
 * 64-bit port: 'regs' are the caller's saved registers (1991: a pointer
 * to the eip on the stack). The caller may be the init task, still in
 * the kernel: exec puts it in user mode by setting cs and ss as well.
 */
int do_execve(struct regs * regs,char * filename,
	char ** argv, char ** envp)
{
	struct m_inode * inode;
	struct buffer_head * bh;
	struct exec ex;
	unsigned long page[MAX_ARG_PAGES];
	int i,argc,envc;
	unsigned long p;

	for (i=0 ; i<MAX_ARG_PAGES ; i++)	/* clear page-table */
		page[i]=0;
	if (!(inode=namei(filename)))		/* get executables inode */
		return -ENOENT;
	if (!S_ISREG(inode->i_mode)) {	/* must be regular file */
		iput(inode);
		return -EACCES;
	}
	i = inode->i_mode;
	if (current->uid && current->euid) {
		if (current->euid == inode->i_uid)
			i >>= 6;
		else if (current->egid == inode->i_gid)
			i >>= 3;
	} else if (i & 0111)
		i=1;
	if (!(i & 1)) {
		iput(inode);
		return -ENOEXEC;
	}
	if (!(bh = bread(inode->i_dev,inode->i_zone[0]))) {
		iput(inode);
		return -EACCES;
	}
	ex = *((struct exec *) bh->b_data);	/* read exec-header */
	brelse(bh);
	if (N_MAGIC(ex) != ZMAGIC || N_MACHTYPE(ex) != M_X86_64 ||
		ex.a_trsize || ex.a_drsize ||
		(unsigned long) ex.a_text+ex.a_data+ex.a_bss > 0x80000000UL ||
		inode->i_size < ex.a_text+ex.a_data+ex.a_syms+N_TXTOFF(ex)) {
		iput(inode);
		return -ENOEXEC;
	}
	if (N_TXTOFF(ex) != BLOCK_SIZE)
		panic("N_TXTOFF != BLOCK_SIZE. See a.out.h.");
	argc = count(argv);
	envc = count(envp);
	p = copy_strings(envc,envp,page,PAGE_SIZE*MAX_ARG_PAGES-4);
	p = copy_strings(argc,argv,page,p);
	if (!p) {
		for (i=0 ; i<MAX_ARG_PAGES ; i++)
			free_page(page[i]);
		iput(inode);
		return -1;
	}
/* OK, This is the point of no return */
	for (i=0 ; i<32 ; i++)
		current->sig_fn[i] = NULL;
	for (i=0 ; i<NR_OPEN ; i++)
		if ((current->close_on_exec>>i)&1)
			sys_close(i);
	current->close_on_exec = 0;
	free_page_tables(current->pml4);
	if (last_task_used_math == current)
		last_task_used_math = NULL;
	current->used_math = 0;
	current->fs_kernel = 0;		/* from now on, a user process */
	p += setup_arg_pages(page)-MAX_ARG_PAGES*PAGE_SIZE;
	p = (unsigned long) create_tables((char *)p,argc,envc);
	current->brk = ex.a_bss +
		(current->end_data = ex.a_data +
		(current->end_code = ex.a_text));
	current->start_stack = p & 0xfffff000;
	i = read_area(inode,ex.a_text+ex.a_data);
	iput(inode);
	if (i<0)
		sys_exit(-1);
	i = ex.a_text+ex.a_data;
	while (i&0xfff)
		put_fs_byte(0,(char *) (unsigned long) (i++));
	memset(regs,0,sizeof(*regs));
	regs->rip = ex.a_entry;		/* eip, magic happens :-) */
	regs->rsp = p;			/* stack pointer */
	regs->cs = USER_CS;
	regs->ss = USER_DS;
	regs->rflags = 0x202;		/* interrupts on */
	return 0;
}

long sys_execve(char * filename,char ** argv,char ** envp,struct regs * regs)
{
	return do_execve(regs,filename,argv,envp);
}
