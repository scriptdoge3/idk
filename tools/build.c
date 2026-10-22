/*
 *  tools/build.c
 *
 * Glues the boot sector and the kernel together into a bootable 1.44Mb
 * floppy image.
 *
 * Modern port: the original stripped Minix (boot) and GCC a.out (system)
 * headers. Both inputs are now raw binaries made by the GNU toolchain,
 * so all that is left is checking sizes and padding the result out to a
 * full floppy, which is what the BIOS (and emulators) expect.
 */
#include <stdio.h>	/* fprintf */
#include <stdlib.h>	/* contains exit */
#include <sys/types.h>	/* unistd.h needs this */
#include <unistd.h>	/* contains read/write */
#include <fcntl.h>

#define FLOPPY_SIZE (1440*1024)
#define MAX_SYSTEM (0x90000-0x10000)	/* see boot/boot.s */

void die(char * str)
{
	fprintf(stderr,"%s\n",str);
	exit(1);
}

void usage(void)
{
	die("Usage: build boot system [> image]");
}

int main(int argc, char ** argv)
{
	int i,c,id,sys,total;
	char buf[1024];

	if (argc != 3)
		usage();
	for (i=0;i<sizeof buf; i++) buf[i]=0;
	if ((id=open(argv[1],O_RDONLY,0))<0)
		die("Unable to open 'boot'");
	i=read(id,buf,sizeof buf);
	fprintf(stderr,"Boot sector %d bytes.\n",i);
	if (i!=512)
		die("Boot block must be exactly 512 bytes");
	if ((unsigned char) buf[510]!=0x55 || (unsigned char) buf[511]!=0xAA)
		die("Boot block lacks the 0x55AA signature");
	i=write(1,buf,512);
	if (i!=512)
		die("Write call failed");
	close (id);

	if ((id=open(argv[2],O_RDONLY,0))<0)
		die("Unable to open 'system'");
	for (i=0 ; (c=read(id,buf,sizeof buf))>0 ; i+=c )
		if (write(1,buf,c)!=c)
			die("Write call failed");
	close(id);
	sys = i;
	fprintf(stderr,"System %d bytes.\n",sys);
	if (sys > MAX_SYSTEM)
		die("System is too big for the boot loader");
	for (i=0;i<sizeof buf; i++) buf[i]=0;
	for (total = 512+sys ; total < FLOPPY_SIZE ; total += c) {
		c = FLOPPY_SIZE-total;
		if (c > sizeof buf)
			c = sizeof buf;
		if (write(1,buf,c)!=c)
			die("Write call failed");
	}
	return(0);
}
