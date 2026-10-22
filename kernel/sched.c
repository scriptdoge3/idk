/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <signal.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
	struct task_struct task;
	char stack[TASK_BLOCK];
};

static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task), *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };

/*
 * 1991's task 0 ran in user mode on user_stack. 64-bit port: it stays
 * in the kernel, on the stack in its own task_union, which head.s
 * loads from here.
 */
char * stack_start = init_task.stack + TASK_BLOCK;

struct tss_struct tss;
static unsigned long double_fault_stack[512];

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 *
 * 64-bit port: the device-not-available trap comes here directly (TS is
 * set by switch_task). fxsave/fxrstor cover the fpu and sse registers.
 */
void math_state_restore()
{
	static unsigned int mxcsr = 0x1f80;	/* all sse exceptions masked */

	__asm__ volatile ("clts");
	if (last_task_used_math == current)
		return;
	if (last_task_used_math)
		__asm__ volatile ("fxsave %0":"=m" (last_task_used_math->fxsave));
	if (current->used_math)
		__asm__ volatile ("fxrstor %0"::"m" (current->fxsave));
	else {
		__asm__ volatile ("fninit ; ldmxcsr %0"::"m" (mxcsr));
		current->used_math=1;
	}
	last_task_used_math=current;
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if ((*p)->signal && (*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

long sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

void do_timer(long cpl)
{
	if (cpl)
		current->utime++;
	else
		current->stime++;
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;
	schedule();
}

long sys_alarm(long seconds)
{
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return seconds;
}

long sys_getpid(void)
{
	return current->pid;
}

long sys_getppid(void)
{
	return current->father;
}

long sys_getuid(void)
{
	return current->uid;
}

long sys_geteuid(void)
{
	return current->euid;
}

long sys_getgid(void)
{
	return current->gid;
}

long sys_getegid(void)
{
	return current->egid;
}

long sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

long sys_signal(long signal,long addr,long restorer)
{
	long i;

	switch (signal) {
		case SIGHUP: case SIGINT: case SIGQUIT: case SIGILL:
		case SIGTRAP: case SIGABRT: case SIGFPE: case SIGUSR1:
		case SIGSEGV: case SIGUSR2: case SIGPIPE: case SIGALRM:
		case SIGCHLD:
			i=(long) current->sig_fn[signal-1];
			current->sig_fn[signal-1] = (fn_ptr) addr;
			current->sig_restorer = (fn_ptr) restorer;
			return i;
		default: return -1;
	}
}

/*
 * 64-bit port: one tss for everybody. Its rsp0 is where the cpu puts
 * the stack on an interrupt from user mode; switch_task keeps it at the
 * top of the current task's kernel stack.
 */
void sched_init(void)
{
	int i;
	unsigned long base = (unsigned long) &tss, limit = sizeof(tss)-1;

	for(i=1;i<NR_TASKS;i++)
		task[i] = NULL;
	tss.rsp0 = (unsigned long) stack_start;
	tss.ist[0] = (unsigned long) (double_fault_stack+512);
	tss.iomap_base = sizeof(tss);	/* no io bitmap: no ports for users */
	tss_desc[0] = (limit & 0xffff) | ((base & 0xffffff) << 16) |
		(0x89UL << 40) | (((limit >> 16) & 0xf) << 48) |
		(((base >> 24) & 0xff) << 56);
	tss_desc[1] = base >> 32;
	__asm__ volatile ("ltr %%ax"::"a" (TSS_SEL));
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);
}

/* the assembly code knows these (asm/regs.inc): fail to compile if wrong */
#define ASM_OFFSET(name,cond) typedef char name[(cond) ? 1 : -1]
ASM_OFFSET(check_signal,__builtin_offsetof(struct task_struct,signal) == 24);
ASM_OFFSET(check_ksp,__builtin_offsetof(struct task_struct,ksp) == 32);
ASM_OFFSET(check_cr3,__builtin_offsetof(struct task_struct,cr3) == 40);
ASM_OFFSET(check_regs,sizeof(struct regs) == 0xb0 &&
	__builtin_offsetof(struct regs,rax) == 0x70 &&
	__builtin_offsetof(struct regs,cs) == 0x90);
ASM_OFFSET(check_rsp0,__builtin_offsetof(struct tss_struct,rsp0) == 4 &&
	sizeof(struct tss_struct) == 104);
ASM_OFFSET(check_block,TASK_BLOCK == 8192 &&
	sizeof(struct task_struct) <= TASK_BLOCK/2);
