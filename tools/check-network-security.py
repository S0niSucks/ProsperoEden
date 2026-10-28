#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Docker-only, loopback Samba with disposable accounts; never prints credentials/configs."""
import json
import os
from pathlib import Path
import secrets
import socket
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = Path(os.environ.get('NETWORK_WORK', '/work/network-check')) / 'security'


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    WORK.mkdir(parents=True, exist_ok=True)
    Path('/run/samba/ncalrpc').mkdir(parents=True, exist_ok=True)
    WORK.chmod(0o700)
    share = WORK / 'share'; share.mkdir(exist_ok=True); share.chmod(0o755)
    (share / 'Proof.nro').write_bytes(b'0123456789abcdef')
    # The account exists only in this disposable container, not on the host or NAS.
    user = 'pefixture'
    subprocess.run(['useradd', '-M', '-s', '/usr/sbin/nologin', user], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    password = secrets.token_urlsafe(32)
    build = WORK.parent / 'build'
    subprocess.run(['cmake', '-S', str(ROOT / 'headless/network'), '-B', str(build),
                    '-DEDEN_NETWORK_DEPS=/work/deps', '-DEDEN_NETWORK_CHECK=ON'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--target', 'eden-network-security-check', '-j2'], check=True)
    # force user avoids granting public traversal of our private credential/test directory.
    for max_protocol in ('SMB3', 'SMB2_10'):
        conf = WORK / 'smb.conf'
        for d in ('lock', 'state', 'cache', 'private', 'logs'): (WORK / d).mkdir(exist_ok=True)
        conf.write_text(f'''[global]
server role = standalone server
interfaces = 127.0.0.1
bind interfaces only = yes
smb ports = 1446
disable netbios = yes
server min protocol = SMB2
server max protocol = {max_protocol}
server signing = mandatory
map to guest = Bad User
load printers = no
printing = bsd
printcap name = /dev/null
pid directory = {WORK}
lock directory = {WORK / 'lock'}
state directory = {WORK / 'state'}
cache directory = {WORK / 'cache'}
private dir = {WORK / 'private'}
log file = {WORK / 'logs/log.%m'}
[signed]
path = {share}
read only = yes
guest ok = no
valid users = {user}
force user = root
[sealed]
path = {share}
read only = yes
guest ok = no
valid users = {user}
force user = root
smb encrypt = required
[guest]
path = {share}
read only = yes
guest ok = yes
force user = root
''')
        conf.chmod(0o600)
        subprocess.run(['smbpasswd', '-s', '-a', user, '-c', str(conf)],
                       input=password + '\n' + password + '\n', text=True, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        server = subprocess.Popen(['smbd', '--foreground', '--no-process-group', '-s', str(conf)],
                                  stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        try:
            for _ in range(100):
                try:
                    with socket.create_connection(('127.0.0.1', 1446), timeout=.1): break
                except OSError: time.sleep(.02)
            cases = [('signed', user, password, False, True),
                     ('signed', user, password, True, max_protocol == 'SMB3'),
                     ('sealed', user, password, True, max_protocol == 'SMB3'),
                     ('signed', user, password + 'wrong', False, False),
                     ('guest', 'nonexistent-fixture', password, False, False),
                     ('signed', '', '', False, False)]
            for target, login, secret, encrypted, expected in cases:
                config = WORK / 'shares.json'
                config.write_text(json.dumps({'shares': [dict(id='test', protocol='smb', server='127.0.0.1',
                    share=target, user=login, password=secret, encrypt=encrypted, port=1446, timeout_ms=1500)]}))
                config.chmod(0o600)
                result = subprocess.run([str(build / 'eden-network-security-check'), str(config), 'yes' if expected else 'no'],
                                        capture_output=True, timeout=20)
                if result.returncode: raise RuntimeError(f'{max_protocol}: {target}, expected={expected}: check failed (private output suppressed)')
            print(f'{max_protocol}: authenticated mandatory-signing, encryption requirement, bad auth, guest downgrade refusal PASS')
        finally:
            server.terminate(); server.wait(timeout=5)
            (WORK / 'shares.json').unlink(missing_ok=True)
    subprocess.run(['smbpasswd', '-x', user, '-c', str(conf)], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(['userdel', user], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


if __name__ == '__main__': main()
