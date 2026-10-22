/* chmod octal-mode file... */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
	int i, mode, status = 0;
	char * end;

	if (argc < 3) {
		fprintf(stderr, "usage: chmod octal-mode file...\n");
		return 2;
	}
	mode = strtol(argv[1], &end, 8);
	if (*end) {
		fprintf(stderr, "chmod: only octal modes are supported\n");
		return 2;
	}
	for (i = 2; i < argc; i++)
		if (chmod(argv[i], mode) < 0) {
			perror(argv[i]);
			status = 1;
		}
	return status;
}
