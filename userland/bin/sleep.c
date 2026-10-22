/* sleep seconds */
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static void wake(int sig)
{
}

int main(int argc, char ** argv)
{
	int n;

	if (argc != 2 || (n = atoi(argv[1])) < 0) {
		fprintf(stderr, "usage: sleep seconds\n");
		return 2;
	}
	if (n) {
		signal(SIGALRM, wake);
		alarm(n);
		pause();
	}
	return 0;
}
