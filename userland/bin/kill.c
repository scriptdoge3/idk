/* kill [-signal] pid... */
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
	int i = 1, sig = SIGTERM, status = 0;

	if (argc > 1 && argv[1][0] == '-') {
		sig = atoi(argv[1] + 1);
		i++;
	}
	if (i == argc) {
		fprintf(stderr, "usage: kill [-signal] pid...\n");
		return 2;
	}
	for ( ; i < argc; i++)
		if (kill(atoi(argv[i]), sig) < 0) {
			perror(argv[i]);
			status = 1;
		}
	return status;
}
