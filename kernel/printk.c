/*
 * When in kernel-mode, we cannot use printf, as fs is liable to
 * point to 'interesting' things. Make a printf with fs-saving, and
 * all is well.
 *
 * 64-bit port: "fs-saving" is now the task's fs_kernel flag (see
 * <asm/segment.h>), set while tty_write() reads our buffer.
 */
#include <stdarg.h>
#include <stddef.h>

#include <linux/kernel.h>
#include <linux/sched.h>

extern int vsprintf(char * buf, const char * fmt, va_list args);

static char buf[1024];

int printk(const char *fmt, ...)
{
	va_list args;
	int i, old;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	old = current->fs_kernel;
	current->fs_kernel = 1;
	tty_write(0,buf,i);
	current->fs_kernel = old;
	return i;
}
