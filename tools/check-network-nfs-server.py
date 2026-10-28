#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Ganesha protocol server using MEM FSAL, without privileged Docker capabilities.
Not a NAS/disk qualification: Ganesha VFS FSAL requires open_by_handle_at privileges.
"""
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get('NETWORK_WORK', '/work/network-check')) / 'ganesha-mem'


def stop_child(child, name):
    method = 'SIGTERM'
    try:
        try:
            child.terminate()
        except PermissionError:
            # A foreground service may drop UID. This Popen child has not been reaped,
            # so its exact PID cannot be recycled while the same-ID helper signals it.
            status = dict(line.split(':', 1) for line in Path(f'/proc/{child.pid}/status').read_text().splitlines())
            if int(status['PPid']) != os.getpid():
                raise RuntimeError(f'{name}: refusing to signal a process that is not our child')
            uid, gid = int(status['Uid'].split()[0]), int(status['Gid'].split()[0])
            method = f'same-ID SIGTERM helper uid={uid} gid={gid}'
            print(f'{name} cleanup: pid={child.pid} {method}', flush=True)
            subprocess.run([sys.executable, '-B', '-c',
                            'import os, signal, sys; os.kill(int(sys.argv[1]), signal.SIGTERM)', str(child.pid)],
                           user=uid, group=gid, extra_groups=[], check=True, timeout=5)
    finally:
        code = child.wait(timeout=5)
    if code not in (0, -signal.SIGTERM):
        raise RuntimeError(f'{name}: unexpected service exit {code}')
    print(f'{name} cleanup complete: pid={child.pid} method={method} wait_exit={code}', flush=True)


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    if {name for _, name in socket.if_nameindex()} != {'lo'}:
        raise SystemExit('Ganesha MEM requires Docker --network none (loopback-only namespace)')
    WORK.mkdir(parents=True, exist_ok=True); Path('/run/ganesha').mkdir(exist_ok=True)
    build = WORK.parent / 'build'
    subprocess.run(['cmake', '-S', str(ROOT / 'headless/network'), '-B', str(build),
                    '-DEDEN_NETWORK_DEPS=/work/deps', '-DEDEN_NETWORK_CHECK=ON'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'eden-network-nfs-server-check', '-j2'], check=True)
    conf = WORK / 'ganesha.conf'
    # Explicit numeric Bind_addr uses AI_ADDRCONFIG, which rejects loopback-only namespaces.
    # The server default bind is safe only in the isolated namespace required above; the
    # client still connects to 127.0.0.1 and the fixture continues to use MEM, never disk VFS.
    conf.write_text(f'''NFS_CORE_PARAM {{ Protocols = 3; NFS_Port = 12050; MNT_Port = 12051; }}
MEM {{ Inode_Size = 1048576; }}
EXPORT {{ Export_Id = 1; Path = /games; Pseudo = /games; Access_Type = RW;
Protocols = 3; Transports = TCP; SecType = sys; Squash = No_Root_Squash;
FSAL {{ Name = MEM; }} }}
LOG {{ Default_Log_Level = WARN; Facility {{ name = FILE; destination = {WORK / 'server.log'}; enable = active; }} }}
''')
    rpc = subprocess.Popen(['rpcbind', '-f', '-w'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        server = subprocess.Popen(['ganesha.nfsd', '-F', '-f', str(conf), '-p', str(WORK / 'pid')],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                try:
                    with socket.create_connection(('127.0.0.1', 12051), timeout=.1): break
                except OSError: time.sleep(.05)
            subprocess.run([str(build / 'eden-network-nfs-server-check')], check=True, timeout=30)
        finally:
            stop_child(server, 'ganesha')
    finally:
        stop_child(rpc, 'rpcbind')


if __name__ == '__main__': main()
