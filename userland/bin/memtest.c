/*
 * memtest [Mb] - exercise the 64-bit memory management and the fpu:
 *
 * - allocate and fill a block (default 64Mb), pages appear on demand;
 * - fork: the child rewrites the whole block, which must not show in
 *   the parent (copy-on-write);
 * - meanwhile both sum a harmonic series in floating point, being
 *   switched in and out by the timer; their results must agree to the
 *   last bit (the fpu/sse state follows each process).
 */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static double harmonic(long n)
{
	double h = 0;
	long i;

	for (i = 1; i <= n; i++)
		h += 1.0 / i;
	return h;
}

#define PATTERN(i) ((unsigned long) (i) * 0x9e3779b97f4a7c15UL)

int main(int argc, char ** argv)
{
	long mb = argc > 1 ? atoi(argv[1]) : 64, words, i, bad = 0;
	unsigned long * p;
	double h, hc;
	int fds[2], pid, status;

	words = (mb << 20) / sizeof(unsigned long);
	if (!(p = malloc(words * sizeof(unsigned long)))) {
		perror("memtest: malloc");
		return 1;
	}
	printf("memtest: %ld Mb at %p, stack near %p\n", mb, (void *) p,
		(void *) &mb);
	for (i = 0; i < words; i++)
		p[i] = PATTERN(i);
	if (pipe(fds) < 0 || (pid = fork()) < 0) {
		perror("memtest");
		return 1;
	}
	if (!pid) {
		for (i = 0; i < words; i++)
			p[i] = ~PATTERN(i);
		hc = harmonic(3000000);
		write(fds[1], (char *) &hc, sizeof hc);
		for (i = 0; i < words; i++)
			if (p[i] != ~PATTERN(i))
				_exit(2);
		_exit(0);
	}
	h = harmonic(3000000);
	if (read(fds[0], (char *) &hc, sizeof hc) != sizeof hc)
		hc = -1;
	waitpid(pid, &status, 0);
	for (i = 0; i < words; i++)
		if (p[i] != PATTERN(i))
			bad++;
	printf("memtest: child status %d, %ld words changed under the parent\n",
		status >> 8, bad);
	printf("memtest: fpu %s (H(3e6) = %ld.%06ld)\n",
		h == hc ? "agrees" : "DISAGREES", (long) h,
		(long) ((h - (long) h) * 1000000));
	if (status || bad || h != hc) {
		printf("memtest: FAILED\n");
		return 1;
	}
	printf("memtest: ok\n");
	return 0;
}
