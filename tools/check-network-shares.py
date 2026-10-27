#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused Docker-only check. Real libsmb2 -> loopback Samba; real libnfs -> synthetic RFC1813
TCP server. No mount, broadcast, host/private service, game, key or user account is used.
Needs smbd and the pinned client source trees. Set NETWORK_WORK to a private Docker volume path.
"""
import os
import socket
import socketserver
import struct
import subprocess
import threading
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SIZE = (5 << 30) + 65536
WORK = Path(os.environ.get("NETWORK_WORK", "/work/network-check"))


def pattern(offset, size):
    return bytes(0 if (4 << 20) <= n < (5 << 30) else (n * 13 + (n >> 17)) % 251
                 for n in range(offset, offset + size))


def u32(n): return struct.pack(">I", n)
def u64(n): return struct.pack(">Q", n)
def opaque(b): return u32(len(b)) + b + b"\0" * (-len(b) % 4)


class Reader:
    def __init__(self, data): self.data, self.at = data, 0
    def u32(self):
        n, = struct.unpack_from(">I", self.data, self.at); self.at += 4; return n
    def u64(self):
        n, = struct.unpack_from(">Q", self.data, self.at); self.at += 8; return n
    def opaque(self):
        size = self.u32(); data = self.data[self.at:self.at+size]; self.at += (size + 3) // 4 * 4; return data


def attr(handle):
    directory = handle in (b"root", b"folder")
    size = 0 if directory else SIZE
    return b"".join(u32(n) for n in (2 if directory else 1, 0o755 if directory else 0o644, 1, 65534, 65534)) + \
        u64(size) + u64(size) + u32(0) + u32(0) + u64(1) + u64(1 if directory else 2) + \
        (u32(1700000000) + u32(0)) * 3


def post(handle): return u32(1) + attr(handle)


class Nfs(socketserver.BaseRequestHandler):
    """Small deterministic NFSv3/MOUNT TCP fixture, not a mock of our Session API."""
    def handle(self):
        def receive(size):
            data = bytearray()
            while len(data) < size:
                chunk = self.request.recv(size-len(data))
                if not chunk: raise EOFError
                data.extend(chunk)
            return bytes(data)
        try:
            while True:
                marker, = struct.unpack(">I", receive(4))
                assert marker & 0x80000000 and marker & 0x7fffffff < 1048576
                r = Reader(receive(marker & 0x7fffffff))
                xid = r.u32(); assert r.u32() == 0 and r.u32() == 2
                program, version, proc = r.u32(), r.u32(), r.u32()
                r.u32(); r.opaque(); r.u32(); r.opaque()  # credential and verifier
                body = self.call(program, version, proc, r)
                reply = b"".join(u32(n) for n in (xid, 1, 0, 0, 0, 0)) + body
                self.request.sendall(u32(0x80000000 | len(reply)) + reply)
        except (EOFError, ConnectionError):
            pass

    def call(self, program, version, proc, r):
        if proc == 0: return b""
        if program == 100005:
            assert version == 3 and proc == 1
            if r.opaque() != b"/games": return u32(13)  # MNT3ERR_ACCES
            return u32(0) + opaque(b"root") + u32(1) + u32(1)
        assert program == 100003 and version == 3, (program, version, proc)
        handle = r.opaque()
        if proc == 1: return u32(0) + attr(handle)  # GETATTR
        if proc == 3:  # LOOKUP
            name = r.opaque()
            if name not in (b"Game.nsp", b"Folder", b"Nested.nro"):
                return u32(2) + post(handle)
            child = b"folder" if name == b"Folder" else name
            return u32(0) + opaque(child) + post(child) + post(handle)
        if proc == 4: return u32(0) + post(handle) + u32(r.u32())  # ACCESS: return granted requested bits
        if proc == 6:  # READ
            offset, count = r.u64(), r.u32()
            if offset == 2 << 30:  # designated in-flight cancellation read, never reply
                while self.request.recv(65536): pass
                raise EOFError
            count = min(count, max(0, SIZE - offset))
            return u32(0) + post(handle) + u32(count) + u32(offset+count >= SIZE) + opaque(pattern(offset, count))
        if proc in (16, 17):  # READDIR[PLUS]
            result = u32(0) + post(handle) + b"\0"*8
            names = [(b"Game.nsp", b"Game.nsp"), (b"Folder", b"folder")] if handle == b"root" else [(b"Nested.nro", b"Nested.nro")]
            for index, (name, child) in enumerate(names):
                result += u32(1) + u64(index+1) + opaque(name) + u64(index+1)
                if proc == 17: result += post(child) + u32(1) + opaque(child)
            return result + u32(0) + u32(1)
        if proc == 19:  # FSINFO
            return u32(0) + post(handle) + b"".join(u32(n) for n in (262144, 262144, 4096, 262144, 262144, 4096, 8192)) + \
                u64(1 << 44) + u32(0) + u32(1) + u32(0x1b)
        if proc == 20: return u32(0) + post(handle) + b"".join(u32(n) for n in (32000, 255, 1, 1, 0, 1))
        raise AssertionError((program, version, proc))


class Stalled(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            while self.request.recv(65536): pass  # deliberately never reply
        except ConnectionError:
            pass


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    if not Path("/.dockerenv").exists(): raise SystemExit("Run this focused check inside Docker, not on the host/WSL")
    WORK.mkdir(parents=True, exist_ok=True)
    Path("/run/samba/ncalrpc").mkdir(parents=True, exist_ok=True)
    share = WORK / "share"; share.mkdir(exist_ok=True)
    (share / "Folder").mkdir(exist_ok=True)
    for file in (share / "Game.nsp", share / "Truncate.nsp", share / "Folder/Nested.nro"):
        with file.open("wb") as out:
            out.write(pattern(0, 4 << 20)); out.truncate(SIZE); out.seek(5 << 30); out.write(pattern(5 << 30, 65536))
        file.chmod(0o644)
    share.chmod(0o755)
    for name in ("lock", "state", "cache", "private", "logs"):
        (WORK / name).mkdir(exist_ok=True)
    config = WORK / "smb.conf"
    config.write_text(f"""[global]
server role = standalone server
interfaces = 127.0.0.1
bind interfaces only = yes
smb ports = 1445
disable netbios = yes
map to guest = Bad User
server min protocol = SMB2
load printers = no
printing = bsd
printcap name = /dev/null
pid directory = {WORK}
lock directory = {WORK / 'lock'}
state directory = {WORK / 'state'}
cache directory = {WORK / 'cache'}
private dir = {WORK / 'private'}
log file = {WORK / 'logs/log.%m'}
[games]
path = {share}
read only = yes
guest ok = yes
wide links = no
follow symlinks = no
""")
    deps = Path(os.environ.get("EDEN_NETWORK_DEPS", "/work/deps"))
    sanitized = os.environ.get("NETWORK_SANITIZE") == "1"
    build = WORK / ("build-sanitized" if sanitized else "build")
    flags = ["-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer",
             "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer"] if sanitized else []
    subprocess.run(["cmake", "-S", str(ROOT / "headless/network"), "-B", str(build),
                    f"-DEDEN_NETWORK_DEPS={deps}", "-DEDEN_NETWORK_CHECK=ON", "-DCMAKE_BUILD_TYPE=Debug", *flags], check=True)
    subprocess.run(["cmake", "--build", str(build), "--target", "eden-network-check", "-j2"], check=True)
    smbd = subprocess.Popen(["smbd", "--foreground", "--no-process-group", "--configfile", str(config)],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    nfs = Server(("127.0.0.1", 12049), Nfs)
    stalled = Server(("127.0.0.1", 19998), Stalled)
    for server in (nfs, stalled): threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        import time
        for _ in range(100):
            try:
                with socket.create_connection(("127.0.0.1", 1445), timeout=0.1): break
            except OSError: time.sleep(0.02)
        for protocol, port, name, mount in (("smb", 1445, "games", 0), ("nfs", 12049, "/games", 12049)):
            subprocess.run([str(build / "eden-network-check"), protocol, str(port), name, str(mount), str(WORK)],
                           check=True, timeout=90)
    finally:
        smbd.terminate(); smbd.wait(timeout=5)
        for server in (nfs, stalled): server.shutdown(); server.server_close()


if __name__ == "__main__": main()
