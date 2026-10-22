# Linux 0.01, 64-bit

This is Linus Torvalds' Linux 0.01 from September 1991, taken from the
[kernel.org historic archive](https://cdn.kernel.org/pub/linux/kernel/Historic/linux-0.01.tar.gz)
and ported to x86-64. The kernel runs in 64-bit long mode, the programs are
64-bit, and every byte of RAM the BIOS reports is usable, including memory
above 4 GB (up to a configurable 64 GB). It boots to a working shell in
QEMU.

It is still recognisably 0.01: the scheduler, the file system, the tty
code, the buffer cache and most of the rest are the 1991 code. What had to
be redesigned is the part 64-bit mode took away (segmentation and hardware
task switching), described below.

The history tells the story:
- The first commit is the untouched 1991 tarball.
- `8b8a4bb` is a 32-bit port that only modernises the toolchain.
- The commits after it are the 64-bit port.

![Linux 0.01 running in 64-bit mode in QEMU](docs/screenshot.png)

## Quick start

You need:
- `gcc` and binutils for x86-64 (the normal packages on any x86-64 Linux)
- GNU `make`
- `python3`
- `qemu-system-x86_64`

```sh
make run            # build everything, boot it in QEMU with 512 MB
make run MEM=8G     # ... or with more memory
make run-curses     # in the terminal (Alt-2 for the QEMU monitor, then "quit")
make test           # headless boot with 6 GB + a scripted shell session
                    # (TESTMEM=512M if the host can't spare 6 GB)
```

`make` builds two images:

- **`Image`**: a 1.44 MB boot floppy holding the boot sector and the kernel.
- **`hd.img`**: a 32 MB hard disk with one Minix partition, the root file
  system.

## What you can do

Init starts `/bin/sh` on the console. The shell supports:
- pipes, and redirection with `<`, `>` and `>>`
- background jobs (`&`), and `;`, `&&` and `||`
- quoting, `$VAR`, `$?` and `$$`
- `*` and `?` globbing

Its built-in commands are `cd`, `pwd`, `export`, `unset`, `exit` and `help`.

| | |
|---|---|
| files | `ls [-la]` `cat` `cp` `mv` `rm [-rf]` `ln` `mkdir` `rmdir` `touch` `chmod` |
| text | `echo` `wc [-lwc]` `head [-n]` |
| system | `uname [-a]` `free` `date` `sync` `kill` `sleep` `env` `clear` `hello` |
| tests | `memtest [Mb]` |

Things to try:

- `uname -a`: the machine is `x86_64`.
- `free`: how much memory there is. Boot with `MEM=8G` to see more than 4 GB.
- `memtest 256`: allocates 256 MB, forks, and checks that the child's
  writes don't show in the parent (copy-on-write). Both processes also
  do floating-point work at the same time and must get the same result.
- `echo hi > f; cat f | wc`
- `cat > notes`: type some lines, then end with Ctrl-D.

Ctrl-C interrupts the running program. If you `exit` the shell, init
starts a new one.

The 1991 kernel never writes the buffer cache back on its own. The `update`
daemon (started by init, as Linus intended) runs `sync` every 30 seconds.
Run `sync` yourself before closing QEMU if you want to keep your changes.
`make hd.img` rebuilds the disk from scratch.

## How the 64-bit port works

**Booting.** The floppy boot sector still loads the kernel with the BIOS
and switches to 32-bit protected mode, as in 1991. It also asks the BIOS
for its memory map (`int 0x15`, `e820`) and leaves it for the kernel.

`boot/head.s` then sets up page tables, enables PAE and long mode, and
jumps to the kernel. The kernel is linked at `0xffffffff80000000` but loaded
at physical address 0.

**The address space.** In 1991 every process was a 64 MB segment of one
shared address space, told apart by segment bases. Long mode has no
segment bases, so now:

- Each process has its own 4-level page tables.
- The process sees itself at address 0 upwards, as before, but with 512 GB
  of room instead of 64 MB. Its stack starts just under 512 GB.
- The kernel occupies the top half of every address space. All physical
  memory is mapped there at `0xffff800000000000`.

The mapping means `get_free_page()` can still return a plain pointer, and
the rest of the kernel didn't have to care where a page lives. `mem_map`,
1991's page reference counts, is sized from the BIOS map at boot.
`MAX_MEMORY` in `include/linux/config.h` caps the RAM used: the counts cost
2 bytes per 4 kB page.

`fork()` copies the page tables and shares every page read-only, and the
first write copies it (copy-on-write). This is 1991's `copy_page_tables`
and `do_wp_page`, one level deeper.

**Processes.** Long mode has no hardware task switch. These replace it:

- `switch_task` in `kernel/system_call.s` saves the registers C needs kept,
  swaps kernel stacks and loads the next address space.
- There is one TSS, which only holds the kernel stack pointer for the next
  entry from user mode.
- Each task has an 8 kB block: its `task_struct` with the kernel stack
  above it.
- FPU and SSE state is still switched lazily, 0.01's
  `math_state_restore()`, now with `fxsave`/`fxrstor`.

**Task 0 and init.** In 1991, task 0 dropped to user mode and forked init,
both running kernel code. Kernel code can't run in user mode without
segments, so now:

1. Task 0 stays in the kernel as the idle loop.
2. Init starts as a kernel task, mounts root, opens the console, and execs
   `/bin/init` into user mode.
3. `/bin/init` runs the old spawn-a-shell loop, moved out of `init/main.c`.

**System calls** still go through `int $0x80`, with the number in `%rax` and
up to three arguments in `%rbx`, `%rcx` and `%rdx`. The registers are now
64 bits wide. `struct regs` in `include/linux/sched.h` describes the saved
registers, which `fork` and `execve` work on.

**Reaching user memory.** The kernel used to reach a process's memory
through `%fs`, whose segment limit kept a process from pointing the kernel
at kernel memory. Now the kernel uses user pointers directly, and
`<asm/segment.h>` checks them instead. A bad pointer gets the process a
SIGSEGV.

**Signals** are delivered by `kernel/signal.c` (1991: assembly). The frame
on the user stack is unwound by a restorer that ends with a user-mode
`iretq`. That instruction puts back rip, the flags and rsp in one go.

**Unchanged.** The formats stay those of 1991:
- the Minix v1 file system
- the 1 kB-header ZMAGIC a.out executables

An a.out header now marks x86-64 code in its machine-type byte, so a 1991
binary is refused rather than run as the wrong kind of code.

### 1991 bugs the port turned up

- `insert_into_queues()` in `fs/buffer.c` wrote through a NULL pointer
  whenever a hash chain was empty. In 1991 that hit an unused page-directory
  entry. Now address 0 belongs to the running process, and the write
  corrupted its code.
- `exec()` loaded binaries over about 520 kB to the wrong offset (a
  double-indirect block offset), and its argument-overflow check could
  never be true.
- `get_free_page()` scanned all of `mem_map` for every page. That was fine
  for 1,500 pages, but with millions it made large allocations quadratic.

### From the earlier modern-toolchain port

These came with the first, 32-bit port and still apply:

- **Build.** GNU as and ld replace as86 and Minix. `-fleading-underscore`
  keeps the `_symbol` names the assembly uses.
- **Inline assembly.** All of it has correct constraints. gcc 1.x let an
  `asm` statement clobber its own inputs; gcc today miscompiles that.
- **Plain C.** `<string.h>` is C, and `<stdarg.h>` uses the compiler
  builtins.
- **Disk.** The IDE driver selects the drive before polling it.
- **Interrupt races.** Three are fixed, in the disk queue, the console and
  the tty line counter.
- **Clock.** The CMOS date handling is fixed (Y2K, and BCD months).
- **Small changes.** A US keyboard map, with Linus' Finnish one available
  through `--defsym KBD_FINNISH=1`. init respawns the shell, as in 0.11.

Still 1991, besides the code:
- 64 tasks at most
- the `0133` umask
- `uname`'s placeholder fields, apart from the machine field
- Minix v1 file systems with 14-character names
- no `rename()`, symlinks or `mknod()`

## The userland (`userland/`)

In 1991 Linux borrowed its programs from Minix. This repository has its own:

- **`userland/lib`**: a small C library: syscall stubs, stdio, malloc,
  directories and time. It is built against the kernel's own `include/`,
  which was also the libc header set at the time.
- **`userland/bin`**: `init`, the shell and the programs listed above.
- **`userland/mkaout.py`**: turns the linked 64-bit ELF files into the
  ZMAGIC a.out format that `exec()` loads.
- **`tools/mkrootfs.py`**: writes the disk image (MBR, Minix v1 file system,
  device nodes from `userland/devices`) without root, loop devices or
  `mkfs.minix`.

To add a program, put `foo.c` in `userland/bin`, add `foo` to `PROGS` in
`userland/Makefile`, and run `make hd.img`.

## Running elsewhere

It runs on anything that emulates a PC with a 64-bit CPU: QEMU, Bochs or
86Box. It needs:
- a BIOS that boots floppies
- an IDE disk on the legacy ports (0x1F0, IRQ 14)
- a VGA text console

On real hardware you would need legacy BIOS boot, a disk controller in IDE
compatibility mode, and the exact disk geometry hard-coded in `config.h`.
Without a 64-bit CPU, `head.s` prints a message and stops.

If you change the disk geometry, change it in two places: `HD_TYPE` in
`include/linux/config.h`, and `HD_CYLS`/`HD_HEADS`/`HD_SECS` in the
`Makefile`.

## Debugging

`tools/system` is a 64-bit ELF file with symbols. To debug the kernel in
QEMU:

```sh
make run QEMUEXTRA="-s -S"         # in one terminal
gdb tools/system -ex 'target remote :1234'
```

C symbols carry a leading underscore (`b _schedule`).
