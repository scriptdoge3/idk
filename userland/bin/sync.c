/* sync - flush the buffer cache to disk */
#include <unistd.h>

int main(void)
{
	sync();
	return 0;
}
