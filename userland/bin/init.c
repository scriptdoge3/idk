/*
 * init - process 1's program.
 *
 * This is the loop that was init() in the 1991 init/main.c (with the
 * respawning Linux 0.11 added). The 64-bit kernel can't run its own code
 * in user mode, so after mounting the root file system and opening the
 * console it execs this instead.
 */
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static char * argv[] = { "-",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

int main(void)
{
	int pid, i, j;

	if (!fork())
		_exit(execve("/bin/update",NULL,NULL));
	for (;;) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		} else if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while ((j=wait(&i)) != pid && j != -1)
			/* e.g. /bin/update */ ;
		printf("child %d died with code %04x\n\r",j,i);
		sync();
	}
}
