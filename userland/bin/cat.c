/* cat [file...] */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

static int copy(int fd)
{
	char buf[1024];
	int n;

	while ((n = read(fd, buf, sizeof buf)) > 0)
		if (write(1, buf, n) != n)
			return -1;
	return n;
}

int main(int argc, char ** argv)
{
	int i, fd, status = 0;

	if (argc < 2)
		return copy(0) < 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && !argv[i][1])
			fd = 0;
		else if ((fd = open(argv[i], O_RDONLY)) < 0) {
			perror(argv[i]);
			status = 1;
			continue;
		}
		if (copy(fd) < 0) {
			perror(argv[i]);
			status = 1;
		}
		if (fd)
			close(fd);
	}
	return status;
}
