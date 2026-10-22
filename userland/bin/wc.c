/* wc [-lwc] [file...] - lines, words, bytes */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static long tl, tw, tc;
static int lflag, wflag, cflag;

static void report(long l, long w, long c, const char * name)
{
	const char * fmt = "%7ld";

	if (lflag) {
		printf(fmt, l);
		fmt = " %7ld";
	}
	if (wflag) {
		printf(fmt, w);
		fmt = " %7ld";
	}
	if (cflag)
		printf(fmt, c);
	printf("%s%s\n", name ? " " : "", name ? name : "");
}

static void count(int fd, const char * name)
{
	char buf[1024];
	long l = 0, w = 0, c = 0;
	int n, i, inword = 0;

	while ((n = read(fd, buf, sizeof buf)) > 0)
		for (i = 0, c += n; i < n; i++) {
			if (buf[i] == '\n')
				l++;
			if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n')
				inword = 0;
			else if (!inword) {
				inword = 1;
				w++;
			}
		}
	report(l, w, c, name);
	tl += l; tw += w; tc += c;
}

int main(int argc, char ** argv)
{
	int i, fd, status = 0, first;
	char * p;

	for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++)
		for (p = argv[i] + 1; *p; p++)
			if (*p == 'l')
				lflag = 1;
			else if (*p == 'w')
				wflag = 1;
			else if (*p == 'c')
				cflag = 1;
	if (!lflag && !wflag && !cflag)
		lflag = wflag = cflag = 1;
	first = i;
	if (i == argc)
		count(0, NULL);
	for ( ; i < argc; i++) {
		if ((fd = open(argv[i], O_RDONLY)) < 0) {
			perror(argv[i]);
			status = 1;
			continue;
		}
		count(fd, argv[i]);
		close(fd);
	}
	if (argc - first > 1)
		report(tl, tw, tc, "total");
	return status;
}
