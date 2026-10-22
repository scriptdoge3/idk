/* echo [-n] [arg...] */
#include <string.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	int i = 1, nl = 1;

	if (argc > 1 && !strcmp(argv[1], "-n")) {
		nl = 0;
		i++;
	}
	for ( ; i < argc; i++)
		printf("%s%s", argv[i], i < argc - 1 ? " " : "");
	if (nl)
		putchar('\n');
	return 0;
}
