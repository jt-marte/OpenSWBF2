"""List annotated globals that the decomp declares without an initializer but whose
retail .data bytes are non-zero (a decompiled zero where retail had a value).

Usage: python -I tools/global_init_scan.py <retail BattlefrontII.exe> <repo>/src

Reads only the exe. Static class members declared in a header show up too; check the .cpp
definition, which may already carry the value. Found archFilter, BOOT_JOYSTICK_BITS,
netReplicaOk, netRelayOk and lastFilledTurn.
"""
import os
import re
import struct
import sys

exe = sys.argv[1]
src = sys.argv[2]
data = open(exe, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
nsec = struct.unpack_from('<H', data, pe + 6)[0]
optsz = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    s = pe + 24 + optsz + 40 * i
    name = data[s:s + 8].rstrip(b'\0').decode()
    vsize, vaddr, rsize, raddr = struct.unpack_from('<IIII', data, s + 8)
    secs.append((name, base + vaddr, vsize, rsize, raddr))


def read(va, n):
    for name, start, vsize, rsize, raddr in secs:
        if start <= va < start + vsize:
            off = va - start
            if off + n <= rsize:
                return name, data[raddr + off:raddr + off + n]
            return name, None
    return None, None


SIZES = {'bool': 1, 'char': 1, 'unsigned char': 1, 'short': 2, 'unsigned short': 2,
         'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'float': 4,
         'unsigned __int64': 8, '__int64': 8, 'double': 8, 'DWORD': 4, 'BOOL': 4}
decl = re.compile(r'^\s*(?:static\s+)?((?:unsigned\s+)?(?:__int64|int|char|short|long|bool|float|double|DWORD|BOOL))\s+(\w+)\s*;')
glob = re.compile(r'//\s*GLOBAL:\s*SWBF2\s+(0x[0-9a-fA-F]+)')
for root, _, files in os.walk(src):
    for fn in files:
        if not fn.endswith(('.cpp', '.h')):
            continue
        path = os.path.join(root, fn)
        lines = open(path, encoding='latin-1').read().splitlines()
        for i, line in enumerate(lines):
            m = glob.search(line)
            if not m:
                continue
            for j in range(i + 1, min(i + 4, len(lines))):
                if lines[j].strip().startswith('//'):
                    continue
                d = decl.match(lines[j])
                if d:
                    typ, name = d.group(1), d.group(2)
                    n = SIZES.get(re.sub(r'\s+', ' ', typ), 4)
                    sec, raw = read(int(m.group(1), 16), n)
                    if raw and any(raw):
                        if typ == 'float':
                            val = struct.unpack('<f', raw)[0]
                        elif n == 8:
                            val = struct.unpack('<Q', raw)[0]
                        else:
                            val = int.from_bytes(raw, 'little', signed=True)
                        print(f'{os.path.relpath(path, src)}:{j + 1}: {typ} {name} retail={val} ({m.group(1)} {sec})')
                break
