/*
 *  'fork.c' contains the help-routines for the 'fork' system call
 * (see also system_call.s), and some misc functions ('verify_area').
 * Fork is rather simple, once you get the hang of it, but the memory
 * management can be a bitch. See 'mm/mm.c': 'copy_page_tables()'
 */
#include <errno.h>
#include <signal.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);
extern unsigned long * new_pml4(void);
extern void ret_from_fork(void);
extern void kernel_thread_start(void);

long last_pid=0;

int find_empty_process(void);

/* a bad pointer from a process: see <asm/segment.h> */
void fs_fault(void)
{
	current->signal |= (1<<(SIGSEGV-1));
}

void verify_area(void * addr,long size)
{
	unsigned long start;

	if (!FS_OK(addr,size)) {
		fs_fault();
		return;
	}
	start = (unsigned long) addr;
	if (start >= TASK_SIZE)		/* kernel data, see fs_kernel */
		return;
	size += start & 0xfff;
	start &= ~0xfffUL;
	while (size>0) {
		size -= 4096;
		write_verify(start);
		start += 4096;
	}
}

/*
 * 1991 gave the child the next 64Mb segment and copied the page tables
 * into it. 64-bit port: the child gets an address space of its own,
 * sharing every page with its parent until one of them writes.
 */
int copy_mem(int nr,struct task_struct * p)
{
	if (!(p->pml4 = new_pml4()))
		return -ENOMEM;
	p->cr3 = PHYS(p->pml4);
	if (copy_page_tables(current->pml4,p->pml4)) {
		free_page_tables(p->pml4);
		free_page((unsigned long) p->pml4);
		return -ENOMEM;
	}
	return 0;
}

static void free_task(struct task_struct * p)
{
	free_page((unsigned long) p);
	free_page(PAGE_SIZE + (unsigned long) p);
}

/* everything but the stack and the address space of a new task */
static struct task_struct * new_task(void)
{
	struct task_struct *p;

	p = (struct task_struct *) get_free_pages2();
	if (!p)
		return NULL;
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	p->state = TASK_RUNNING;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	return p;
}

static void copy_files(struct task_struct * p)
{
	int i;
	struct file *f;

	for (i=0; i<NR_OPEN;i++)
		if (f=p->filp[i])
			f->f_count++;
	if (current->pwd)
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 *
 * 64-bit port: "the necessary registers" are a copy of the parent's
 * struct regs at the top of the child's kernel stack, with 0 in rax,
 * and under it a frame for switch_task that returns to ret_from_fork,
 * which takes the child back to user mode like any system call.
 */
int copy_process(int nr,struct regs * regs)
{
	struct task_struct *p;
	struct regs * childregs;
	unsigned long * sp;
	int i;

	if (!(p = new_task()))
		return -EAGAIN;
	if (last_task_used_math == current)
		__asm__ volatile ("fxsave %0":"=m" (p->fxsave));
	childregs = ((struct regs *) (TASK_BLOCK + (unsigned long) p)) - 1;
	*childregs = *regs;
	childregs->rax = 0;
	sp = (unsigned long *) childregs;
	*--sp = (unsigned long) ret_from_fork;
	*--sp = 0x2;		/* rflags: interrupts off until the iret */
	for (i=0 ; i<6 ; i++)
		*--sp = 0;	/* rbp, rbx, r12 - r15 */
	p->ksp = (unsigned long) sp;
	if (copy_mem(nr,p)) {
		free_task(p);
		return -EAGAIN;
	}
	copy_files(p);
	task[nr] = p;	/* do this last, just in case */
	return last_pid;
}

long sys_fork(long rbx,long rcx,long rdx,struct regs * regs)
{
	long nr = find_empty_process();

	if (nr < 0)
		return nr;
	return copy_process(nr,regs);
}

/*
 * 64-bit port: 1991's init was task 0 forking itself in user mode,
 * sharing the kernel's memory. Without segments, kernel code can't run
 * in user mode, so init starts as a task that runs 'fn' in the kernel,
 * with an empty address space, until it execs.
 */
int start_kernel_task(void (*fn)(void))
{
	struct task_struct *p;
	unsigned long * sp;
	int nr, i;

	if ((nr = find_empty_process()) < 0)
		return nr;
	if (!(p = new_task()))
		return -EAGAIN;
	p->fs_kernel = 1;
	if (!(p->pml4 = new_pml4())) {
		free_task(p);
		return -ENOMEM;
	}
	p->cr3 = PHYS(p->pml4);
	sp = (unsigned long *) (TASK_BLOCK + (unsigned long) p);
	*--sp = (unsigned long) kernel_thread_start;
	*--sp = 0x2;			/* rflags */
	*--sp = 0;			/* rbp */
	*--sp = (unsigned long) fn;	/* rbx */
	for (i=0 ; i<4 ; i++)
		*--sp = 0;		/* r12 - r15 */
	p->ksp = (unsigned long) sp;
	copy_files(p);
	task[nr] = p;
	return last_pid;
}

int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}
