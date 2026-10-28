#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real Ganesha protocol server using MEM FSAL, without privileged Docker capabilities.
Not a NAS/disk qualification: Ganesha VFS FSAL requires open_by_handle_at privileges.
"""
import os
from pathlib import Path
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get('NETWORK_WORK', '/work/network-check')) / 'ganesha-mem'


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    WORK.mkdir(parents=True, exist_ok=True); Path('/run/ganesha').mkdir(exist_ok=True)
    build = WORK.parent / 'build'
    subprocess.run(['cmake', '-S', str(ROOT / 'headless/network'), '-B', str(build),
                    '-DEDEN_NETWORK_DEPS=/work/deps', '-DEDEN_NETWORK_CHECK=ON'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'eden-network-nfs-server-check', '-j2'], check=True)
    conf = WORK / 'ganesha.conf'
    conf.write_text(f'''NFS_CORE_PARAM {{ Protocols = 3; Bind_addr = 127.0.0.1; NFS_Port = 12050; MNT_Port = 12051; }}
MEM {{ Inode_Size = 1048576; }}
EXPORT {{ Export_Id = 1; Path = /games; Pseudo = /games; Access_Type = RW;
Protocols = 3; Transports = TCP; SecType = sys; Squash = No_Root_Squash;
FSAL {{ Name = MEM; }} }}
LOG {{ Default_Log_Level = WARN; Facility {{ name = FILE; destination = {WORK / 'server.log'}; enable = active; }} }}
''')
    rpc = subprocess.Popen(['rpcbind', '-f', '-w'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    server = subprocess.Popen(['ganesha.nfsd', '-F', '-f', str(conf), '-p', str(WORK / 'pid')],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1', 12051), timeout=.1): break
            except OSError: time.sleep(.05)
        subprocess.run([str(build / 'eden-network-nfs-server-check')], check=True, timeout=30)
    finally:
        server.terminate(); server.wait(timeout=5); rpc.terminate(); rpc.wait(timeout=5)


if __name__ == '__main__': main()
