/* env - print the environment */
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	char ** e;

	for (e = environ; e && *e; e++)
		puts(*e);
	return 0;
}
