/* free - how much memory there is (the 64-bit port's meminfo call) */
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	unsigned long m[3];

	if (meminfo(m) < 0) {
		perror("meminfo");
		return 1;
	}
	printf("%10s %10s %10s %10s\n", "kB", "total", "used", "free");
	printf("%10s %10lu %10lu %10lu\n", "paged", m[0] >> 10,
		(m[0] - m[1]) >> 10, m[1] >> 10);
	printf("top of physical memory: %p (%lu Mb)\n", (void *) m[2], m[2] >> 20);
	return 0;
}
