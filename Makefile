#
# Makefile for linux.
#
# Modern port: builds with a stock GNU toolchain (gcc + binutils) instead
# of gcc 1.x/gas/gld/as86 under Minix. See Makefile.header for the flags
# and README.md for how to boot the result in QEMU.
#

include Makefile.header

HOSTCC	=cc
LDFLAGS	=-T tools/system.ld -Map System.map -z noexecstack \
	$(shell $(LD) --help 2>/dev/null | grep -q no-warn-rwx-segments && echo --no-warn-rwx-segments)

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
LIBS	=lib/lib.a

.c.s:
	$(CC) $(CFLAGS) \
	-S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-c -o $*.o $<

all:	Image

Image: boot/boot tools/system.bin tools/build
	tools/build boot/boot tools/system.bin > Image

tools/build: tools/build.c
	$(HOSTCC) -O2 -o tools/build tools/build.c

boot/head.o: boot/head.s

tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(LIBS) tools/system.ld
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(LIBS) \
	-o tools/system

tools/system.bin: tools/system
	$(OBJCOPY) -O binary tools/system tools/system.bin

kernel/kernel.o: FORCE
	$(MAKE) -C kernel

mm/mm.o: FORCE
	$(MAKE) -C mm

fs/fs.o: FORCE
	$(MAKE) -C fs

lib/lib.a: FORCE
	$(MAKE) -C lib

FORCE:

# SYSSIZE is the kernel size in 16-byte clicks, rounded up.
boot/boot:	boot/boot.s tools/system.bin
	$(AS16) --defsym SYSSIZE=$$(( ($$(stat -c %s tools/system.bin) + 15) / 16 )) \
		-o boot/boot.o boot/boot.s
	$(LD16) -Ttext 0 -e start --oformat binary -o boot/boot boot/boot.o

#
# Root file system: a hard disk image with one Minix partition holding
# /bin/sh and friends (see userland/). The geometry here must match
# HD_TYPE in include/linux/config.h.
#
HD_CYLS	=64
HD_HEADS=16
HD_SECS	=63

hd.img: FORCE
	$(MAKE) -C userland
	python3 tools/mkrootfs.py --cyls $(HD_CYLS) --heads $(HD_HEADS) \
		--secs $(HD_SECS) --devices userland/devices \
		-o hd.img userland/build/rootfs

QEMU	=qemu-system-x86_64
QEMUEXTRA=
# guest memory; anything over 4G puts processes' pages above 4Gb
MEM	=512M
QEMUFLAGS=-m $(MEM) -boot a $(QEMUEXTRA) -drive file=Image,if=floppy,format=raw \
	-drive file=hd.img,if=none,id=hd0,format=raw \
	-device ide-hd,drive=hd0,bus=ide.0,cyls=$(HD_CYLS),heads=$(HD_HEADS),secs=$(HD_SECS)

run: Image hd.img
	$(QEMU) $(QEMUFLAGS)

run-curses: Image hd.img
	$(QEMU) $(QEMUFLAGS) -display curses

TESTMEM	=6G
test: Image hd.img
	python3 tools/smoketest.py --qemu $(QEMU) --mem $(TESTMEM) \
		--geometry $(HD_CYLS),$(HD_HEADS),$(HD_SECS)

clean:
	rm -f Image System.map tmp_make boot/boot core hd.img
	rm -f init/*.o boot/*.o tools/system tools/system.bin tools/build
	$(MAKE) -C mm clean
	$(MAKE) -C fs clean
	$(MAKE) -C kernel clean
	$(MAKE) -C lib clean
	$(MAKE) -C userland clean

backup: clean
	(cd .. ; tar cf - linux | compress16 - > backup.Z)
	sync

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

.PHONY: all run run-curses test clean backup dep FORCE

### Dependencies:
init/main.o : init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/asm/system.h include/asm/io.h include/stddef.h \
  include/stdarg.h include/fcntl.h 
