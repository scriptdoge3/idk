#!/usr/bin/env python3
"""
mkaout.py ELF OUT - turn a statically linked ELF (linked at address 0,
see user.ld) into the ZMAGIC a.out that Linux 0.01's exec() loads:
a 32-byte header padded to 1024 bytes, then text+data, with bss and the
entry point described in the header.
"""
import struct
import sys

ZMAGIC = 0o413


def main():
    elf = open(sys.argv[1], 'rb').read()
    if elf[:4] != b'\x7fELF' or elf[4] != 1:
        sys.exit("%s: not a 32-bit ELF file" % sys.argv[1])
    entry, phoff = struct.unpack_from('<II', elf, 24)
    phentsize, phnum = struct.unpack_from('<HH', elf, 42)
    image = bytearray()
    mem_end = 0
    for i in range(phnum):
        ptype, off, vaddr, _, filesz, memsz, _, _ = \
            struct.unpack_from('<8I', elf, phoff + i * phentsize)
        if ptype != 1:          # PT_LOAD
            continue
        if filesz:
            if len(image) < vaddr:
                image.extend(b'\0' * (vaddr - len(image)))
            image[vaddr:vaddr + filesz] = elf[off:off + filesz]
        mem_end = max(mem_end, vaddr + memsz)
    text = len(image)
    bss = mem_end - text
    header = struct.pack('<8I', ZMAGIC, text, 0, bss, 0, entry, 0, 0)
    with open(sys.argv[2], 'wb') as f:
        f.write(header.ljust(1024, b'\0'))
        f.write(image)


if __name__ == '__main__':
    main()
