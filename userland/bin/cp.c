/* cp src dst | cp src... dir */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static int isdir(const char * p)
{
	struct stat st;

	return stat(p, &st) == 0 && S_ISDIR(st.st_mode);
}

static const char * base(const char * p)
{
	const char * s = strrchr(p, '/');

	return s ? s + 1 : p;
}

static int copy(const char * from, const char * to)
{
	char buf[1024];
	struct stat st;
	int in, out, n;

	if ((in = open(from, O_RDONLY)) < 0 || fstat(in, &st) < 0) {
		perror(from);
		return 1;
	}
	if (S_ISDIR(st.st_mode)) {
		fprintf(stderr, "cp: %s: is a directory\n", from);
		close(in);
		return 1;
	}
	if ((out = creat(to, st.st_mode & 0777)) < 0) {
		perror(to);
		close(in);
		return 1;
	}
	while ((n = read(in, buf, sizeof buf)) > 0)
		if (write(out, buf, n) != n) {
			perror(to);
			n = -1;
			break;
		}
	close(in);
	close(out);
	return n < 0;
}

int main(int argc, char ** argv)
{
	char path[300];
	int i, status = 0;

	if (argc < 3) {
		fprintf(stderr, "usage: cp src dst | cp src... dir\n");
		return 2;
	}
	if (!isdir(argv[argc - 1])) {
		if (argc != 3) {
			fprintf(stderr, "cp: %s: not a directory\n", argv[argc - 1]);
			return 1;
		}
		return copy(argv[1], argv[2]);
	}
	for (i = 1; i < argc - 1; i++) {
		snprintf(path, sizeof path, "%s/%s", argv[argc - 1], base(argv[i]));
		status |= copy(argv[i], path);
	}
	return status;
}
