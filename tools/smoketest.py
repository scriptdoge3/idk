#!/usr/bin/env python3
"""
smoketest.py - boot Image + hd.img in a headless QEMU, type commands at the
shell and check what appears on the (VGA text) screen.

Used by 'make test'. The disk image is copied first, so the test never
changes hd.img. The screen is read straight out of video memory through
the QEMU monitor, so no display or serial console is needed.
"""
import argparse
import os
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

KEYS = {' ': 'spc', '\n': 'ret', '-': 'minus', '/': 'slash', '.': 'dot',
        ',': 'comma', '=': 'equal', ';': 'semicolon', "'": 'apostrophe',
        '[': 'bracket_left', ']': 'bracket_right', '\\': 'backslash',
        '`': 'grave_accent', '\x7f': 'backspace', '\x03': 'ctrl-c',
        '\x04': 'ctrl-d'}
SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6',
           '&': '7', '*': '8', '(': '9', ')': '0', '_': 'minus', '+': 'equal',
           '{': 'bracket_left', '}': 'bracket_right', '|': 'backslash',
           ':': 'semicolon', '"': 'apostrophe', '<': 'comma', '>': 'dot',
           '?': 'slash', '~': 'grave_accent'}


class Vm:
    def __init__(self, image, hd, geom, qemu):
        self.tmp = tempfile.mkdtemp(prefix='linux001-')
        self.sock = os.path.join(self.tmp, 'monitor')
        disk = os.path.join(self.tmp, 'hd.img')
        shutil.copy(hd, disk)
        cyls, heads, secs = geom
        self.proc = subprocess.Popen(
            [qemu, '-m', '16', '-display', 'none', '-boot', 'a',
             '-drive', 'file=%s,if=floppy,format=raw,readonly=on' % image,
             '-drive', 'file=%s,if=none,id=hd0,format=raw' % disk,
             '-device', 'ide-hd,drive=hd0,bus=ide.0,cyls=%d,heads=%d,secs=%d'
             % (cyls, heads, secs),
             '-monitor', 'unix:%s,server,nowait' % self.sock],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        for _ in range(100):
            if os.path.exists(self.sock):
                break
            time.sleep(0.05)
        self.mon = socket.socket(socket.AF_UNIX)
        self.mon.connect(self.sock)
        self.mon.settimeout(5)
        self.command('')

    def command(self, line):
        self.mon.sendall((line + '\n').encode())
        buf = b''
        while not buf.rstrip().endswith(b'(qemu)') or len(buf) < len(line) + 8:
            buf += self.mon.recv(65536)
        return buf.decode('latin1')

    def type(self, text):
        for ch in text:
            if ch in SHIFTED:
                key = 'shift-' + SHIFTED[ch]
            elif ch.isupper():
                key = 'shift-' + ch.lower()
            else:
                key = KEYS.get(ch, ch)
            self.command('sendkey ' + key)

    def lines(self):
        """Every line of text in the console's video memory (0xb8000-0xc0000)."""
        words = []
        for off in range(0, 0x8000, 0x1000):
            out = self.command('xp /%dhx %#x' % (0x800, 0xb8000 + off))
            for row in out.splitlines():
                m = re.match(r'\s*[0-9a-f]+:\s+(.*)', row)
                if m:
                    words += [int(v, 16) & 0xff for v in m.group(1).split()]
        text = ''.join(chr(c) if 32 <= c < 127 else ' ' for c in words)
        return [text[i:i + 80].rstrip() for i in range(0, len(text), 80)]

    def wait_for(self, check, timeout):
        end = time.time() + timeout
        while time.time() < end:
            lines = self.lines()
            if any(check(l) for l in lines):
                return True
            time.sleep(0.3)
        return False

    def close(self):
        self.proc.kill()
        self.proc.wait()
        shutil.rmtree(self.tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('--image', default='Image')
    ap.add_argument('--hd', default='hd.img')
    ap.add_argument('--geometry', default='64,16,63', help='cyls,heads,secs')
    ap.add_argument('--bin', default='userland/build/bin',
                    help='staged programs, to work out expected output')
    ap.add_argument('--qemu', default='qemu-system-i386')
    a = ap.parse_args()

    nprogs = len(os.listdir(a.bin))
    shsize = os.path.getsize(os.path.join(a.bin, 'sh'))
    tests = [
        ('uname -s', 'linux .0'),
        ('echo pipe-ok | cat', 'pipe-ok'),
        ('echo one two three | wc -w', '%7d' % 3),
        ('echo redirected > /tmp/f; cat /tmp/f', 'redirected'),
        ('echo appended >> /tmp/f; wc -l /tmp/f', '%7d /tmp/f' % 2),
        ('mkdir /tmp/dir && cd /tmp/dir && pwd', '/tmp/dir'),
        ('touch x1 x2 y1; echo x*', 'x1 x2'),
        ('cd /; nosuch; echo "rc=$?"', 'rc=127'),
        ('nosuch || echo or-branch', 'or-branch'),
        ('sleep 1 && echo slept-ok', 'slept-ok'),
        ('hello', 'Hello, world! (from a 1991 kernel)'),
        ('ls /bin | wc -l', '%7d' % nprogs),
        ('cat /bin/sh /bin/sh /bin/sh /bin/sh | wc -c', '%7d' % (4 * shsize)),
        ('cp /bin/sh /tmp/sh2 && cmp=$? && echo "cp=$cmp"', 'cp=0'),
        # several processes queueing disk writes at once (hd.c request list)
        ('cp /bin/sh /tmp/w1 & cp /bin/ls /tmp/w2 & sync; sync; echo sync-ok',
         'sync-ok'),
        ('exit 5', lambda l: re.match(r'child \d+ died with code 0500', l)),
    ]

    vm = Vm(a.image, a.hd, [int(x) for x in a.geometry.split(',')], a.qemu)
    failed = 0
    try:
        prompt = lambda l: l.startswith('[/usr/root]#')
        if not vm.wait_for(prompt, 30):
            print('FAIL: no shell prompt after boot')
            print('\n'.join(l for l in vm.lines() if l))
            return 1
        print('ok   boot to shell prompt')
        for cmd, expect in tests:
            vm.type(cmd + '\n')
            check = expect if callable(expect) else (lambda l, e=expect: l == e)
            if vm.wait_for(check, 15):
                print('ok   %s' % cmd)
            else:
                failed += 1
                print('FAIL %s (expected %r)' % (cmd, expect))
        if vm.wait_for(lambda l: l == '[/usr/root]#', 10):
            print('ok   init started a new shell')
        else:
            failed += 1
            print('FAIL init did not start a new shell')
        if failed:
            print('--- screen ---')
            print('\n'.join(l for l in vm.lines() if l))
    finally:
        vm.close()
    print('%d failed' % failed if failed else 'all passed')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
