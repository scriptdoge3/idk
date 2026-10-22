/* rmdir dir... */
#include <unistd.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	int i, status = 0;

	if (argc < 2) {
		fprintf(stderr, "usage: rmdir dir...\n");
		return 2;
	}
	for (i = 1; i < argc; i++)
		if (rmdir(argv[i]) < 0) {
			perror(argv[i]);
			status = 1;
		}
	return status;
}
