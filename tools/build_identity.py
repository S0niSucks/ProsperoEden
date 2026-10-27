#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""stamp|verify ELF: SHA-256 of the complete ELF with only its identity field zeroed.

Stamp only the packaging copy, never the build cache's executable. The field lives in a
readable PT_LOAD, so the runtime can log it even when its code is execute-only on PS5.
This is build identity, not a signature or an authenticity/anti-tampering claim.
"""
import hashlib
from pathlib import Path
import re
import struct
import sys

MARKER = b'PE_BUILD_SHA256:'
ZERO = b'0' * 64


def identity(data, require_stamped=True):
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('expected a little-endian ELF64')
    phoff, = struct.unpack_from('<Q', data, 32)
    phsize, phnum = struct.unpack_from('<HH', data, 54)
    if phsize < 56 or phnum > 4096 or phoff + phsize * phnum > len(data):
        raise ValueError('invalid ELF program headers')
    slots = set()
    for n in range(phnum):
        kind, flags, offset, _va, _pa, size, _mem, _align = struct.unpack_from('<IIQQQQQQ', data, phoff + n * phsize)
        if offset + size > len(data):
            raise ValueError('ELF segment outside file')
        if kind == 1 and flags & 4 and not flags & 1:  # PT_LOAD, readable, not executable
            for found in re.finditer(re.escape(MARKER) + rb'([0-9a-f]{64})\x00', data[offset:offset + size]):
                slots.add(offset + found.start(1))
    if len(slots) != 1:
        raise ValueError('ELF must have one identity field in a readable non-code segment')
    at = slots.pop()
    expected = hashlib.sha256(data[:at] + ZERO + data[at + 64:]).hexdigest()
    actual = data[at:at + 64].decode('ascii')
    if require_stamped and (actual == ZERO.decode() or actual != expected):
        raise ValueError('ELF identity is unstamped or does not match its bytes')
    return expected, at


def stamp(path):
    data = path.read_bytes()
    digest, at = identity(data, require_stamped=False)
    with path.open('r+b') as file:
        file.seek(at)
        file.write(digest.encode('ascii'))
    assert identity(path.read_bytes())[0] == digest
    return digest


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('stamp', 'verify'):
        sys.exit(__doc__)
    try:
        print(stamp(Path(sys.argv[2])) if sys.argv[1] == 'stamp' else identity(Path(sys.argv[2]).read_bytes())[0])
    except (ValueError, OSError, struct.error) as error:
        sys.exit(str(error))
