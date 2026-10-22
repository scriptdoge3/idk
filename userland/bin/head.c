/* head [-n] [file] */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char ** argv)
{
	int n = 10, i = 1, fd = 0, r, k;
	char buf[1024];

	if (i < argc && argv[i][0] == '-' && argv[i][1]) {
		n = atoi(argv[i] + 1);
		i++;
	}
	if (i < argc && (fd = open(argv[i], O_RDONLY)) < 0) {
		perror(argv[i]);
		return 1;
	}
	while (n > 0 && (r = read(fd, buf, sizeof buf)) > 0) {
		for (k = 0; k < r && n > 0; k++)
			if (buf[k] == '\n')
				n--;
		write(1, buf, k);
	}
	return 0;
}
