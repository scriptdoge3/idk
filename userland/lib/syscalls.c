/*
 * syscalls.c - system call stubs for Linux 0.01.
 *
 * int $0x80 with the call number in %eax and up to three arguments in
 * %ebx, %ecx, %edx. The kernel's C handlers receive those registers as
 * stack arguments and a modern compiler may scribble over them, so the
 * stubs treat all three as clobbered rather than trusting the 1991
 * convention that they come back unchanged.
 */
#define __LIBRARY__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utime.h>
#include <signal.h>
#include <stdarg.h>
#include <unistd.h>

int errno;

static inline long __sys(long nr, long a, long b, long c)
{
	long res;

	__asm__ volatile ("int $0x80"
		: "=a" (res), "+b" (a), "+c" (b), "+d" (c)
		: "0" (nr)
		: "memory");
	if (res < 0) {
		errno = -res;
		return -1;
	}
	return res;
}

#define SYS0(n)		__sys(__NR_##n,0,0,0)
#define SYS1(n,a)	__sys(__NR_##n,(long)(a),0,0)
#define SYS2(n,a,b)	__sys(__NR_##n,(long)(a),(long)(b),0)
#define SYS3(n,a,b,c)	__sys(__NR_##n,(long)(a),(long)(b),(long)(c))

void _exit(int status)
{
	for (;;)
		SYS1(exit,status);
}

int fork(void)			{ return SYS0(fork); }
int read(int fd, char * buf, off_t n)	{ return SYS3(read,fd,buf,n); }
int write(int fd, const char * buf, off_t n) { return SYS3(write,fd,buf,n); }
int close(int fd)		{ return SYS1(close,fd); }
int creat(const char * f, mode_t m)	{ return SYS2(creat,f,m); }
int link(const char * a, const char * b) { return SYS2(link,a,b); }
int unlink(const char * f)	{ return SYS1(unlink,f); }
int chdir(const char * f)	{ return SYS1(chdir,f); }
int chmod(const char * f, mode_t m)	{ return SYS2(chmod,f,m); }
int chown(const char * f, uid_t u, gid_t g) { return SYS3(chown,f,u,g); }
int stat(const char * f, struct stat * s) { return SYS2(stat,f,s); }
int fstat(int fd, struct stat * s)	{ return SYS2(fstat,fd,s); }
int lseek(int fd, off_t off, int whence) { return SYS3(lseek,fd,off,whence); }
int getpid(void)		{ return SYS0(getpid); }
int getppid(void)		{ return SYS0(getppid); }
int getuid(void)		{ return SYS0(getuid); }
int alarm(int sec)		{ return SYS1(alarm,sec); }
int pause(void)			{ return SYS0(pause); }
int utime(const char * f, struct utimbuf * t) { return SYS2(utime,f,t); }
int access(const char * f, mode_t m)	{ return SYS2(access,f,m); }
int sync(void)			{ return SYS0(sync); }
int kill(pid_t pid, int sig)	{ return SYS2(kill,pid,sig); }
int mkdir(const char * f, mode_t m)	{ return SYS2(mkdir,f,m); }
int rmdir(const char * f)	{ return SYS1(rmdir,f); }
int dup(int fd)			{ return SYS1(dup,fd); }
int dup2(int a, int b)		{ return SYS2(dup2,a,b); }
int pipe(int * fds)		{ return SYS1(pipe,fds); }
time_t times(struct tms * t)	{ return SYS1(times,t); }
int uname(struct utsname * u)	{ return SYS1(uname,u); }
mode_t umask(mode_t m)		{ return SYS1(umask,m); }
pid_t setsid(void)		{ return SYS0(setsid); }

/* the optional third argument of these is always a long-sized value */
#define VA3(last) ({ va_list ap; long x; \
	va_start(ap,last); x = va_arg(ap,long); va_end(ap); x; })

int ioctl(int fd, int cmd, ...)		{ return SYS3(ioctl,fd,cmd,VA3(cmd)); }
int fcntl(int fd, int cmd, ...)		{ return SYS3(fcntl,fd,cmd,VA3(cmd)); }
int open(const char * f, int flag, ...)	{ return SYS3(open,f,flag,VA3(flag)); }

int execve(const char * f, char ** argv, char ** envp)
{
	return SYS3(execve,f,argv,envp);
}
pid_t waitpid(pid_t pid, int * stat, int options)
{
	return SYS3(waitpid,pid,stat,options);
}
pid_t wait(int * stat)
{
	return waitpid(-1,stat,0);
}
time_t time(time_t * t)
{
	return SYS1(time,t);
}

/* sys_brk returns the new break, or the old one if it refused */
int brk(void * end)
{
	return SYS1(brk,end) == (long) end ? 0 : -1;
}

void * sbrk(ptrdiff_t incr)
{
	extern char end[];
	static char * cur = end;
	char * old = cur;

	if (incr && brk(cur + incr) < 0)
		return (void *) -1;
	cur += incr;
	return old;
}

/*
 * Signals: the kernel calls the handler with a "restorer" as its return
 * address (kernel/system_call.s). The restorer drops the signal number
 * and puts back the registers the kernel saved on the user stack.
 */
extern void __sig_restore(void);
__asm__(".text\n"
	"__sig_restore:\n\t"
	"addl $4,%esp\n\t"
	"popl %eax\n\t"
	"popl %ecx\n\t"
	"popl %edx\n\t"
	"popfl\n\t"
	"ret");

void (*signal(int sig, void (*fn)(int)))(int)
{
	long res;
	long a = sig, b = (long) fn, c = (long) __sig_restore;

	__asm__ volatile ("int $0x80"
		: "=a" (res), "+b" (a), "+c" (b), "+d" (c)
		: "0" (__NR_signal)
		: "memory");
	return (void (*)(int)) res;
}
