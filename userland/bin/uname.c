/* uname [-asnrvm] */
#include <sys/utsname.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	struct utsname u;
	const char * f = argc > 1 ? argv[1] : "-s";
	int first = 1;

	if (uname(&u) < 0) {
		perror("uname");
		return 1;
	}
	if (strchr(f, 'a'))
		f = "-snrvm";
#define FIELD(c,v) if (strchr(f,c)) { printf(first ? "%s" : " %s", v); first = 0; }
	FIELD('s', u.sysname)
	FIELD('n', u.nodename)
	FIELD('r', u.release)
	FIELD('v', u.version)
	FIELD('m', u.machine)
	putchar('\n');
	return 0;
}
