/* date - print the time the kernel read from the CMOS clock */
#include <time.h>
#include <stdio.h>

int main(void)
{
	time_t t = time(NULL);

	fputs(ctime(&t), stdout);
	return 0;
}
