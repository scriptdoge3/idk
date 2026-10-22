/*
 * mv src dst | mv src... dir
 *
 * Linux 0.01's rename() is ENOSYS, so this is link() + unlink(), which
 * means directories can't be moved.
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static int move(const char * from, const char * to)
{
	struct stat st;

	if (stat(to, &st) == 0 && !S_ISDIR(st.st_mode))
		unlink(to);
	if (link(from, to) < 0) {
		perror(from);
		return 1;
	}
	if (unlink(from) < 0) {
		perror(from);
		return 1;
	}
	return 0;
}

int main(int argc, char ** argv)
{
	struct stat st;
	char path[300], * b;
	int i, status = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: mv src dst | mv src... dir\n");
		return 2;
	}
	if (stat(argv[argc - 1], &st) < 0 || !S_ISDIR(st.st_mode)) {
		if (argc != 3) {
			fprintf(stderr, "mv: %s: not a directory\n", argv[argc - 1]);
			return 1;
		}
		return move(argv[1], argv[2]);
	}
	for (i = 1; i < argc - 1; i++) {
		b = strrchr(argv[i], '/');
		snprintf(path, sizeof path, "%s/%s", argv[argc - 1], b ? b + 1 : argv[i]);
		status |= move(argv[i], path);
	}
	return status;
}
