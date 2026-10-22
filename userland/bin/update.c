/*
 * update - started by init (init/main.c) as /bin/update. Linux 0.01
 * never writes dirty buffers back on its own, so this daemon calls
 * sync() every 30 seconds.
 */
#include <signal.h>
#include <unistd.h>

static void tick(int sig)
{
}

int main(void)
{
	signal(SIGINT, SIG_IGN);
	for (;;) {
		signal(SIGALRM, tick);
		alarm(30);
		pause();
		sync();
	}
}
