/* ln target linkname (hard links only - 0.01 has no symlinks) */
#include <unistd.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: ln target linkname\n");
		return 2;
	}
	if (link(argv[1], argv[2]) < 0) {
		perror(argv[2]);
		return 1;
	}
	return 0;
}
