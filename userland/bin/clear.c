/* clear - home the cursor and erase the screen (console.c knows ESC [ H/J) */
#include <unistd.h>

int main(void)
{
	write(1, "\033[H\033[J", 6);
	return 0;
}
