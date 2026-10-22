#
# Userland for Linux 0.01: a tiny libc and a handful of programs, built as
# ZMAGIC a.out binaries and staged in build/rootfs for tools/mkrootfs.py.
#
# The C headers are the kernel's own include/ (which in 1991 doubled as
# the libc headers), plus a few extras in include/ here.
#

AS	=as --64 --noexecstack
LD	=ld -m elf_x86_64
AR	=ar
CC	=gcc
CFLAGS	=-m64 -Os -std=gnu89 \
	-ffreestanding -fno-builtin -nostdinc -Iinclude -I../include \
	-fno-pic -fno-pie -fno-stack-protector -fcf-protection=none \
	-fno-asynchronous-unwind-tables -fno-unwind-tables \
	-Wall -Wno-main -Wno-unused-function
LDFLAGS	=-T user.ld -z noexecstack \
	$(shell $(LD) --help 2>/dev/null | grep -q no-warn-rwx-segments && echo --no-warn-rwx-segments)

PROGS	=init sh ls cat echo mkdir rmdir rm ln cp mv touch uname date sync \
	update kill chmod wc sleep clear env hello head free memtest
LIBOBJS	=lib/syscalls.o lib/libc.o lib/string.o lib/time.o

all: build/rootfs

build/lib.a: $(LIBOBJS)
	@mkdir -p build
	$(AR) rcs $@ $(LIBOBJS)

lib/%.o: lib/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

lib/crt0.o: lib/crt0.s
	$(AS) -o $@ $<

build/%.o: bin/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.elf: build/%.o lib/crt0.o build/lib.a user.ld
	$(LD) $(LDFLAGS) -o $@ lib/crt0.o $< build/lib.a

build/bin/%: build/%.elf mkaout.py
	@mkdir -p build/bin
	python3 mkaout.py $< $@

build/rootfs: $(PROGS:%=build/bin/%) $(shell find skel)
	rm -rf build/rootfs
	cp -r skel build/rootfs
	mkdir -p build/rootfs/bin build/rootfs/dev build/rootfs/tmp build/rootfs/usr/root
	cp $(PROGS:%=build/bin/%) build/rootfs/bin/
	chmod 755 build/rootfs/bin/*
	touch build/rootfs

clean:
	rm -rf build lib/*.o

.PHONY: all clean
.PRECIOUS: build/%.o build/%.elf
