#define __LIBRARY__
#include <unistd.h>
#include <time.h>

/*
 * 1991: "we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed." Task 0 went
 * to user mode and forked init, both running this file's code.
 *
 * 64-bit port: kernel code can't run in user mode any more (no segments
 * to make the kernel's memory a user's), so task 0 stays in the kernel
 * as the idle task, and init is started as a kernel task that execs
 * /bin/init. The fork-and-respawn loop that was here lives there now.
 * init uses these system calls from kernel mode, by int $0x80 as ever.
 */
static inline _syscall0(int,setup)
extern inline __attribute__((always_inline)) _syscall0(int,pause)

#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void hd_init(void);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;
extern int start_kernel_task(void (*fn)(void));
extern void calc_mem(void);

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)

static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	time.tm_mon--;			/* after BCD_TO_BIN: 0x10-1 isn't 9 */
	BCD_TO_BIN(time.tm_year);
	if (time.tm_year < 70)		/* the CMOS only has two digits: Y2K */
		time.tm_year += 100;
	startup_time = kernel_mktime(&time);
}

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
	mem_init();
	time_init();
	tty_init();
	trap_init();
	sched_init();
	buffer_init();
	hd_init();
	pml4[0] = 0;	/* head.s's identity map: that's user space now */
	__asm__ volatile ("movq %%rax,%%cr3"::"a" (current->cr3):"memory");
	sti();
	calc_mem();
	if (start_kernel_task(init) < 0)
		panic("Can't start init");
/*
 *   NOTE!!   Task 0 gets activated at every idle moment (when no
 * other tasks can run). 1991's task 0 did that with 'pause()'; here
 * it checks if some other task can run, and if not waits for the next
 * interrupt.
 */
	for(;;) {
		schedule();
		__asm__ volatile ("hlt");
	}
}

static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	return i;
}

static char * argv[] = { "-",NULL };
static char * init_argv[] = { "init",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	setup();
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf(" Ok.\n\r");
	execve("/bin/init",init_argv,envp);
	execve("/bin/sh",argv,envp);	/* no init: just a shell, then */
	printf("No /bin/init or /bin/sh to run\n\r");
	for (;;)
		pause();
}
