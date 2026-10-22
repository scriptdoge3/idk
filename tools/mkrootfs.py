#!/usr/bin/env python3
"""
mkrootfs.py - build a hard disk image holding a Linux 0.01 root file system.

Linux 0.01 only understands the original Minix file system (v1, 1kB
blocks, 14-character names) on an AT hard disk with a PC partition table,
and it hard-codes the disk geometry (include/linux/config.h). This tool
writes such a disk: an MBR with one partition and a Minix v1 file system
inside it, populated from a directory on the host. No root, loop devices
or mkfs.minix needed.

Device nodes can't live in a git checkout, so they are described in a
small text file instead (see --devices).
"""
import argparse
import os
import stat
import struct
import sys
import time

BLOCK = 1024
NAME_LEN = 14
DIRENT = 2 + NAME_LEN
INODE_SIZE = 32
INODES_PER_BLOCK = BLOCK // INODE_SIZE
SUPER_MAGIC = 0x137F
# 7 direct + 512 indirect + 512*512 double indirect zones
MAX_SIZE = (7 + 512 + 512 * 512) * BLOCK

S_IFDIR, S_IFREG, S_IFCHR, S_IFBLK = 0o040000, 0o100000, 0o020000, 0o060000


class Fs:
    def __init__(self, nblocks, ninodes):
        if nblocks > 0xffff:
            nblocks = 0xffff        # s_nzones is 16 bits
        self.nzones = nblocks
        self.ninodes = min(ninodes, 0xffff)
        bits = BLOCK * 8
        self.imap_blocks = (self.ninodes + 1 + bits - 1) // bits
        self.zmap_blocks = (self.nzones + bits - 1) // bits
        if self.imap_blocks > 8 or self.zmap_blocks > 8:
            raise SystemExit("file system too big for Linux 0.01 (8 map blocks max)")
        itable = (self.ninodes + INODES_PER_BLOCK - 1) // INODES_PER_BLOCK
        self.first_data = 2 + self.imap_blocks + self.zmap_blocks + itable
        self.img = bytearray(self.nzones * BLOCK)
        self.next_inode = 1
        self.next_zone = self.first_data
        self.now = int(time.time())
        self.inodes = {}            # nr -> dict(mode, uid, gid, size, nlinks, zones)

    # --- allocation -------------------------------------------------------
    def alloc_inode(self, mode, uid=0, gid=0):
        nr = self.next_inode
        if nr > self.ninodes:
            raise SystemExit("out of inodes")
        self.next_inode += 1
        self.inodes[nr] = dict(mode=mode, uid=uid, gid=gid, size=0, nlinks=0,
                               zones=[0] * 9)
        return nr

    def alloc_zone(self):
        z = self.next_zone
        if z >= self.nzones:
            raise SystemExit("out of disk space")
        self.next_zone += 1
        return z

    def put_block(self, z, data):
        self.img[z * BLOCK:z * BLOCK + len(data)] = data

    # --- file data ----------------------------------------------------------
    def write_data(self, nr, data):
        ino = self.inodes[nr]
        nblocks = (len(data) + BLOCK - 1) // BLOCK
        if len(data) > MAX_SIZE:
            raise SystemExit("file too large")
        zones = []
        for i in range(nblocks):
            z = self.alloc_zone()
            self.put_block(z, data[i * BLOCK:(i + 1) * BLOCK])
            zones.append(z)
        ino['size'] = len(data)
        direct, rest = zones[:7], zones[7:]
        ino['zones'][:len(direct)] = direct
        if rest:
            ind, rest = rest[:512], rest[512:]
            ino['zones'][7] = self.indirect(ind)
        if rest:
            tables = []
            while rest:
                tables.append(self.indirect(rest[:512]))
                rest = rest[512:]
            ino['zones'][8] = self.indirect(tables)

    def indirect(self, zones):
        z = self.alloc_zone()
        self.put_block(z, struct.pack('<%dH' % len(zones), *zones))
        return z

    # --- directories ----------------------------------------------------------
    def mkdir(self, parent, mode=0o755):
        nr = self.alloc_inode(S_IFDIR | mode)
        self.inodes[nr]['entries'] = [('.', nr), ('..', parent or nr)]
        self.inodes[nr]['nlinks'] = 2
        if parent:
            self.inodes[parent]['nlinks'] += 1
        return nr

    def link(self, dirnr, name, nr):
        if len(name.encode()) > NAME_LEN:
            raise SystemExit("name too long for Minix v1 (14 chars): " + name)
        self.inodes[dirnr]['entries'].append((name, nr))
        self.inodes[nr]['nlinks'] += 1

    def lookup(self, dirnr, name):
        for n, nr in self.inodes[dirnr]['entries']:
            if n == name:
                return nr
        return None

    def flush_dirs(self):
        for nr, ino in self.inodes.items():
            if 'entries' in ino:
                data = b''.join(struct.pack('<H', i) + n.encode().ljust(NAME_LEN, b'\0')
                                for n, i in ino['entries'])
                self.write_data(nr, data)

    # --- final image ----------------------------------------------------------
    def finish(self):
        self.flush_dirs()
        sb = struct.pack('<6HIH', self.ninodes, self.nzones, self.imap_blocks,
                         self.zmap_blocks, self.first_data, 0, MAX_SIZE, SUPER_MAGIC)
        self.put_block(1, sb)
        # inode bitmap: bit 0 is reserved, bit n is inode n
        self.put_block(2, self.bitmap(self.imap_blocks, self.next_inode,
                                      self.ninodes + 1))
        # zone bitmap: bit 0 reserved, bit n is zone first_data+n-1
        used = self.next_zone - self.first_data + 1
        self.put_block(2 + self.imap_blocks,
                       self.bitmap(self.zmap_blocks, used,
                                   self.nzones - self.first_data + 1))
        base = 2 + self.imap_blocks + self.zmap_blocks
        for nr, ino in self.inodes.items():
            raw = struct.pack('<HHIIBB9H', ino['mode'], ino['uid'], ino['size'],
                              self.now, ino['gid'], ino['nlinks'], *ino['zones'])
            off = base * BLOCK + (nr - 1) * INODE_SIZE
            self.img[off:off + INODE_SIZE] = raw
        return self.img

    @staticmethod
    def bitmap(nblocks, used, valid):
        """First `used` bits set; bits past `valid` also set (don't exist)."""
        bits = bytearray(nblocks * BLOCK)
        for i in range(nblocks * BLOCK * 8):
            if i < used or i >= valid:
                bits[i >> 3] |= 1 << (i & 7)
        return bits


def populate(fs, dirnr, path):
    for name in sorted(os.listdir(path)):
        full = os.path.join(path, name)
        st = os.lstat(full)
        perm = stat.S_IMODE(st.st_mode)
        if stat.S_ISDIR(st.st_mode):
            sub = fs.mkdir(dirnr, perm)
            fs.link(dirnr, name, sub)
            fs.inodes[sub]['nlinks'] -= 1      # link() counted it; mkdir did too
            populate(fs, sub, full)
        elif stat.S_ISREG(st.st_mode):
            nr = fs.alloc_inode(S_IFREG | perm)
            with open(full, 'rb') as f:
                fs.write_data(nr, f.read())
            fs.link(dirnr, name, nr)
        else:
            print("skipping %s (not a file or directory)" % full, file=sys.stderr)


def make_path(fs, root, path):
    d = root
    for part in [p for p in path.split('/') if p]:
        nxt = fs.lookup(d, part)
        if nxt is None:
            nxt = fs.mkdir(d)
            fs.link(d, part, nxt)
            fs.inodes[nxt]['nlinks'] -= 1
        d = nxt
    return d


def add_devices(fs, root, spec):
    """Lines: path c|b major minor [mode]"""
    for line in open(spec):
        line = line.split('#')[0].split()
        if not line:
            continue
        path, kind, major, minor = line[:4]
        mode = int(line[4], 8) if len(line) > 4 else 0o666
        d, name = os.path.split(path)
        dirnr = make_path(fs, root, d)
        nr = fs.alloc_inode((S_IFCHR if kind == 'c' else S_IFBLK) | mode)
        fs.inodes[nr]['zones'][0] = (int(major) << 8) | int(minor)
        fs.link(dirnr, name, nr)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('src', help='directory to copy into the root file system')
    ap.add_argument('-o', '--output', required=True)
    ap.add_argument('--cyls', type=int, default=64)
    ap.add_argument('--heads', type=int, default=16)
    ap.add_argument('--secs', type=int, default=63)
    ap.add_argument('--inodes', type=int, default=2048)
    ap.add_argument('--devices', help='device node list (default: SRC/../devices)')
    a = ap.parse_args()

    total = a.cyls * a.heads * a.secs
    start = a.secs                       # partition starts on cylinder 0, head 1
    nsects = total - start
    nblocks = nsects // 2

    fs = Fs(nblocks, a.inodes)
    root = fs.mkdir(0)
    populate(fs, root, a.src)
    devices = a.devices or os.path.join(os.path.dirname(os.path.abspath(a.src)), 'devices')
    if os.path.exists(devices):
        add_devices(fs, root, devices)
    image = fs.finish()

    def chs(lba):
        c, r = divmod(lba, a.heads * a.secs)
        h, s = divmod(r, a.secs)
        c = min(c, 1023)
        return bytes([h, ((c >> 2) & 0xc0) | (s + 1), c & 0xff])

    mbr = bytearray(512)
    entry = bytes([0x80]) + chs(start) + bytes([0x81]) + chs(total - 1) + \
        struct.pack('<II', start, nsects)
    mbr[0x1be:0x1be + 16] = entry
    mbr[510:512] = b'\x55\xaa'

    with open(a.output, 'wb') as f:
        f.write(mbr)
        f.write(b'\0' * (start * 512 - 512))
        f.write(image)
        f.truncate(total * 512)
    used = fs.next_zone - fs.first_data
    print("%s: %d inodes (%d used), %d blocks (%d used for data)" %
          (a.output, fs.ninodes, fs.next_inode - 1, fs.nzones, used))


if __name__ == '__main__':
    main()
