/* touch file... */
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char ** argv)
{
	int i, fd, status = 0;

	for (i = 1; i < argc; i++) {
		if ((fd = open(argv[i], O_WRONLY | O_CREAT, 0644)) < 0) {
			perror(argv[i]);
			status = 1;
			continue;
		}
		close(fd);
		utime(argv[i], NULL);
	}
	return status;
}
