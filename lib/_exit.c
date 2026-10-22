#define __LIBRARY__
#include <unistd.h>

void _exit(int exit_code)
{
	for (;;)
		__asm__ volatile ("int $0x80"::"a" (__NR_exit),"b" (exit_code));
}
