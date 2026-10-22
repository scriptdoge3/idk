#!/usr/bin/env python3
"""
mkaout.py ELF OUT - turn a statically linked ELF (linked at address 0,
see user.ld) into the ZMAGIC a.out that Linux 0.01's exec() loads:
a 32-byte header padded to 1024 bytes, then text+data, with bss and the
entry point described in the header.

64-bit port: the input is ELF64 x86-64, and the header's machine-type
byte says so (M_X86_64 in include/a.out.h), so the kernel won't try to
run a 1991 binary as 64-bit code or the other way round.
"""
import struct
import sys

ZMAGIC = 0o413
M_X86_64 = 64


def main():
    elf = open(sys.argv[1], 'rb').read()
    if elf[:4] != b'\x7fELF' or elf[4] != 2 or struct.unpack_from('<H', elf, 18)[0] != 62:
        sys.exit("%s: not an x86-64 ELF file" % sys.argv[1])
    entry, phoff = struct.unpack_from('<QQ', elf, 24)
    phentsize, phnum = struct.unpack_from('<HH', elf, 54)
    image = bytearray()
    mem_end = 0
    for i in range(phnum):
        ptype, _, off, vaddr, _, filesz, memsz, _ = \
            struct.unpack_from('<IIQQQQQQ', elf, phoff + i * phentsize)
        if ptype != 1:          # PT_LOAD
            continue
        if filesz:
            if len(image) < vaddr:
                image.extend(b'\0' * (vaddr - len(image)))
            image[vaddr:vaddr + filesz] = elf[off:off + filesz]
        mem_end = max(mem_end, vaddr + memsz)
    text = len(image)
    bss = mem_end - text
    if mem_end >= 1 << 32 or entry >= 1 << 32:
        sys.exit("%s: too big for an a.out header" % sys.argv[1])
    header = struct.pack('<8I', (M_X86_64 << 16) | ZMAGIC, text, 0, bss, 0,
                         entry, 0, 0)
    with open(sys.argv[2], 'wb') as f:
        f.write(header.ljust(1024, b'\0'))
        f.write(image)


if __name__ == '__main__':
    main()
