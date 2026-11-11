#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the real release CLI in fresh, temporary synthetic repositories.

No real dist, app, credentials, native tools or external services are used. Only
the fixture's package-image.sh is stubbed: its deterministic payload is NOT PFS
or PS5 evidence. Independent child-process guards detect private-node inspection.
"""
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from network_privacy import private_network_path

SOURCES = ('tools/ci/make-dist.py', 'tools/network_privacy.py',
           'tools/ci/check-ftp-archive-root.py')
PRIVATE = b'SYNTHETIC-PRIVATE-CANARY-NOT-USER-DATA'
PRIVATE_NAME = 'SYNTHETIC-PRIVATE-NAME'
RELEASE = 'Prospero.Eden-Encore-R1'
SENTINEL = b'Existing dist must survive refused input.\r\n'

# This does NOT import the policy being tested. Listing a private node's parent
# is allowed; stat/open/readlink/enumeration of the node or its children is not.
GUARD = r'''
import json, os, sys
nodes = [os.path.normcase(os.path.abspath(p)) for p in json.loads(os.environ['ENCORE_TEST_DENIED'])]
log = os.environ['ENCORE_TEST_GUARD_LOG']
def record(value):
    with open(log, 'a', encoding='utf-8') as stream:
        stream.write(json.dumps(value) + '\n')
def check(event, value):
    if not isinstance(value, (str, bytes, os.PathLike)): return
    path = os.path.normcase(os.path.abspath(os.fsdecode(value)))
    if any(path == node or path.startswith(node + os.sep) for node in nodes):
        record({'event': event})
        raise PermissionError('Synthetic private-node guard')
record({'guard': 'active'})
def audit(event, args):
    if event in ('open', 'os.scandir', 'os.listdir') and args: check(event, args[0])
sys.addaudithook(audit)
for name in ('stat', 'lstat', 'readlink'):
    original = getattr(os, name)
    def guarded(path, *args, _name=name, _original=original, **kwargs):
        check('os.' + _name, path)
        return _original(path, *args, **kwargs)
    setattr(os, name, guarded)
'''

IMAGE_STUB = r'''
import json, os
from pathlib import Path
import sys
folder, output = map(Path, sys.argv[1:])
calls = Path(os.environ['ENCORE_TEST_IMAGE_CALLS'])
with calls.open('a', encoding='utf-8') as stream:
    stream.write(json.dumps({'folder': str(folder), 'output': str(output)}) + '\n')
files = {p.relative_to(folder).as_posix(): p.read_bytes().hex()
         for p in sorted(folder.rglob('*')) if p.is_file()}
data = b'SYNTHETIC-IMAGE-NOT-PFS\n' + json.dumps(files, sort_keys=True).encode()
if os.environ.get('ENCORE_TEST_NONDETERMINISTIC'):
    data += str(len(calls.read_text().splitlines())).encode()
output.write_bytes(data)
'''


def write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def fixture(directory):
    repo = directory / 'repo'
    for name in SOURCES:
        write(repo / name, (ROOT / name).read_bytes())
    write(repo / 'headless/prosperoeden/version.h', b'constexpr auto kAppVersion = "R1";\n')
    docs = {name: ('Synthetic ' + name + '.  \r\n\n').encode() for name in
            ('README.md', 'INSTALL.md', 'SECURITY.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md')}
    docs['README.md'] += b'## Changes in Encore R1\n\nSynthetic release notes.\n\n## Fixture footer\n'
    licenses = {'GPL-3.0-or-later.txt': b'Synthetic license.  \r\n\n',
                'NOTICE': b'Synthetic notice.\n  \n'}
    for name, data in docs.items():
        write(repo / name, data)
    for name, data in licenses.items():
        write(repo / 'LICENSES' / name, data)
    app = repo / 'input/PPSA99008'
    public = {'eboot.bin': b'Synthetic non-executable payload',
              'sce_sys/param.json': b'{"titleId":"PPSA99008"}',
              'legal/LICENSE': docs['LICENSE'],
              'legal/LICENSES/NOTICE': licenses['NOTICE'],
              'ui/lang/en.txt': b'ShadowMount companion.\r\n',
              'ui/lang/en.mo': b'Synthetic binary catalog',
              'config/settings.json': b'{"public":true}',
              'config/nextendo.json': b'public near match',
              'config/nextendo-public/keep.txt': b'public near match',
              'config/network-shares.txt': b'public near match',
              'assets/network-shares.json': b'public outside config',
              'configuration/nextendo/keep.txt': b'public outside config'}
    for name, data in public.items():
        write(app / name, data)
    write(repo / 'dist/keep.sentinel', SENTINEL)
    write(repo / 'tools/ci/package-image.sh',
          b'#!/usr/bin/env bash\nset -eu\n"$ENCORE_TEST_PYTHON" "$ENCORE_TEST_IMAGE_STUB" "$@"\n')
    write(repo / 'image-stub.py', IMAGE_STUB.encode())
    write(repo / 'tools/ci/diagnose-image-repro.py', b'print("Synthetic image mismatch control")\n')
    write(repo / 'guard/sitecustomize.py', GUARD.encode())
    return repo, app, public, docs, licenses


def environment(repo, denied):
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE='1', PYTHONPATH=str(repo / 'guard'),
               ENCORE_TEST_DENIED=json.dumps([str(p.absolute()) for p in denied]),
               ENCORE_TEST_GUARD_LOG=str(repo / 'guard.jsonl'),
               ENCORE_TEST_IMAGE_CALLS=str(repo / 'image-calls.jsonl'),
               ENCORE_TEST_PYTHON=Path(sys.executable).as_posix(),
               ENCORE_TEST_IMAGE_STUB=(repo / 'image-stub.py').as_posix())
    # Avoid Windows' WSL launcher; use the already-installed Git Bash, not an install.
    if os.name == 'nt':
        bash = Path(os.environ.get('ProgramFiles', 'C:/Program Files')) / 'Git/bin/bash.exe'
        if not bash.is_file():
            raise RuntimeError('Existing Git Bash is required for the synthetic image stub on Windows')
        env['PATH'] = str(bash.parent) + os.pathsep + env.get('PATH', '')
    return env


def invoke(repo, app, image_only=False, denied=(), nondeterministic=False):
    env = environment(repo, denied)
    if nondeterministic:
        env['ENCORE_TEST_NONDETERMINISTIC'] = '1'
    command = [sys.executable, '-B', str(repo / 'tools/ci/make-dist.py')]
    command += ['--image-only'] if image_only else []
    # On Windows CreateProcess searches the working directory before System32
    # (its WSL bash.exe); changing only the child's PATH does not select Git Bash.
    cwd = env['PATH'].split(os.pathsep)[0] if os.name == 'nt' else repo
    result = subprocess.run([*command, str(app)], env=env, cwd=cwd,
                            capture_output=True, text=True, timeout=30)
    assert PRIVATE.decode() not in result.stdout + result.stderr, 'Credential body was printed'
    assert PRIVATE_NAME not in result.stdout + result.stderr, 'Private filename was printed'
    events = [json.loads(line) for line in (repo / 'guard.jsonl').read_text().splitlines()]
    assert events and events[0] == {'guard': 'active'}, 'CLI access guard was not installed'
    # Each synthetic image/diagnostic Python subprocess also installs the guard.
    assert all(event == {'guard': 'active'} for event in events), 'CLI attempted to inspect a private node'
    return result


def calls(repo):
    path = repo / 'image-calls.jsonl'
    return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []


def expected_zip(files):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for name, data in files:
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zipped.writestr(info, data)
    return output.getvalue()


def clean_case(directory, image_only):
    repo, app, public, docs, licenses = fixture(directory)
    result = invoke(repo, app, image_only)
    assert result.returncode == 0, result.stderr
    image = repo / 'dist' / (RELEASE + '.ffpfsc')
    assert image.read_bytes().startswith(b'SYNTHETIC-IMAGE-NOT-PFS\n')
    image_files = json.loads(image.read_bytes().split(b'\n', 1)[1])
    assert image_files == {name: data.hex() for name, data in public.items()}, 'Image input bytes changed'
    image_calls = calls(repo)
    assert len(image_calls) == (1 if image_only else 2)
    assert all(Path(call['folder']) == app and Path(call['output']) == image for call in image_calls)
    if image_only:
        assert (repo / 'dist/keep.sentinel').read_bytes() == SENTINEL
        assert sorted(p.name for p in (repo / 'dist').iterdir()) == [RELEASE + '.ffpfsc', 'keep.sentinel']
    else:
        app_files = [('PPSA99008/' + name, public[name]) for name in sorted(public)]
        full_files = app_files + list(docs.items()) + [('LICENSES/' + name, licenses[name]) for name in sorted(licenses)]
        ftp_files = [(name, data) for name, data in app_files if not name.startswith('PPSA99008/legal/')
                     and not (name.startswith('PPSA99008/ui/lang/') and name.endswith('.txt'))]
        for suffix, expected in (('.zip', full_files), ('-FTP.zip', ftp_files)):
            archive = repo / 'dist' / (RELEASE + suffix)
            assert archive.read_bytes() == expected_zip(expected), 'Clean ZIP bytes/metadata/layout changed'
        for line in (repo / 'dist/SHA256SUMS').read_text().splitlines():
            checksum, name = line.split('  ')
            assert hashlib.sha256((repo / 'dist' / name).read_bytes()).hexdigest() == checksum
        assert (repo / 'dist/release-notes.md').read_text() == 'Synthetic release notes.\n'
        check = subprocess.run([sys.executable, '-B', str(repo / SOURCES[2]), str(repo / 'dist')],
                               capture_output=True, text=True, timeout=10)
        assert check.returncode == 0, check.stderr
    for name, data in public.items():
        assert (app / name).read_bytes() == data, 'Staged app was rewritten'
    for name, data in docs.items():
        assert (repo / name).read_bytes() == data, 'Legal/document text was rewritten'
    for name, data in licenses.items():
        assert (repo / 'LICENSES' / name).read_bytes() == data, 'License text was rewritten'


def rejected_case(directory, image_only, setup):
    repo, app, _, _, _ = fixture(directory)
    argument, denied, cleanup = setup(repo, app)
    try:
        result = invoke(repo, argument, image_only, denied)
        assert result.returncode != 0, 'Unsafe input was accepted'
        assert 'Release preflight refused' in result.stderr, result.stderr
        assert not calls(repo), 'Image subprocess ran for unsafe input'
        assert sorted(p.name for p in (repo / 'dist').iterdir()) == ['keep.sentinel'], 'dist was modified'
        assert (repo / 'dist/keep.sentinel').read_bytes() == SENTINEL
    finally:
        cleanup()


def private_setup(name, kind, symlinks):
    def setup(repo, app):
        path = app / name
        path.parent.mkdir(parents=True, exist_ok=True)
        cleanup = lambda: None
        if kind == 'directory':
            write(path / 'hidden/unknown-stage', PRIVATE)
            if symlinks:
                (path / 'broken-link').symlink_to(repo / 'does-not-exist')
            if hasattr(os, 'mkfifo'):
                os.mkfifo(path / 'never-open.fifo')
            if os.name == 'posix':
                path.chmod(0)
                cleanup = lambda: path.chmod(0o700)
        elif kind == 'dangling':
            path.symlink_to(repo / 'does-not-exist')
        else:
            write(path, PRIVATE)
        return app, [path], cleanup
    return setup


def link_setup(kind):
    def setup(repo, app):
        argument = app
        if kind == 'root':
            argument = repo / 'linked-app'
            argument.symlink_to(app, target_is_directory=True)
        elif kind == 'ancestor':
            parent = repo / 'linked-parent'
            parent.symlink_to(app.parent, target_is_directory=True)
            argument = parent / app.name
        elif kind == 'document':
            (repo / 'LICENSE').unlink()
            (repo / 'LICENSE').symlink_to(app / 'eboot.bin')
        elif kind == 'license':
            (repo / 'LICENSES/link').symlink_to(app / 'eboot.bin')
        else:
            target = {'directory': app / 'config', 'file': app / 'eboot.bin',
                      'dangling': repo / 'missing'}[kind]
            (app / 'public-link').symlink_to(target, target_is_directory=kind == 'directory')
        return argument, [], lambda: None
    return setup


def main():
    private_names = ['config/network-shares.json', 'nested/CoNfIg/NETWORK-SHARES.JSON.new',
                     'config/network-shares.jsonanything', 'config/network-shares.json.' + PRIVATE_NAME,
                     'config/nextendo', 'nested/CONFIG/NEXTENDO', 'legal/config/nextendo']
    aliases = ['config./network-shares.json', 'config /network-shares.json.new',
               'nested/CONFIG. /NeXtEnDo. ', r'config\nextendo',
               r'prefix\CONFIG. /NETWORK-SHARES.JSON.tmp']
    for name in private_names + aliases:
        assert private_network_path(name), 'Private alias not recognized'
    for name in ('config/nextendo.json', 'config/nextendo-public/x', 'config/network-shares.txt',
                 'assets/network-shares.json', 'configuration/nextendo/x'):
        assert not private_network_path(name), 'Public near match was denied'
    source_bytes = {name: (ROOT / name).read_bytes() for name in SOURCES}
    runs, skipped = 0, []
    with tempfile.TemporaryDirectory(prefix='encore-export-privacy-') as tmp:
        directory = Path(tmp)
        symlinks = True
        try:
            (directory / 'probe-link').symlink_to(directory / 'missing')
        except (OSError, NotImplementedError):
            symlinks = False
            skipped.append('physical symlinks (host permission unavailable)')
        if os.name != 'posix':
            skipped.append('physical POSIX dot/space/backslash aliases and FIFO (lexical aliases checked)')
        # Prove that all independent access traps actually fire.
        repo, app, _, _, _ = fixture(directory / 'guard-control')
        target = app / 'guarded'
        write(target, PRIVATE)
        control = 'import os\np=' + repr(str(target)) + '\n'
        control += 'for action in (os.stat, os.lstat, os.readlink, os.scandir, open):\n'
        control += '    try: action(p)\n    except PermissionError: pass\n    else: raise AssertionError("Inactive guard")\n'
        result = subprocess.run([sys.executable, '-B', '-c', control], env=environment(repo, [target]),
                                capture_output=True, text=True, timeout=10)
        assert result.returncode == 0, result.stderr
        events = [json.loads(line) for line in (repo / 'guard.jsonl').read_text().splitlines()]
        assert len(events) == 6 and events[0] == {'guard': 'active'}, 'Access traps were not all exercised'
        for image_only in (False, True):
            mode = 'image-only' if image_only else 'all-formats'
            clean_case(directory / mode / 'clean', image_only)
            runs += 1
            for index, name in enumerate(private_names + (aliases if os.name == 'posix' else [])):
                for kind in ('file', 'directory') + (('dangling',) if symlinks else ()):
                    rejected_case(directory / mode / f'private-{index}-{kind}', image_only,
                                  private_setup(name, kind, symlinks))
                    runs += 1
            if symlinks:
                for kind in ('root', 'ancestor', 'file', 'directory', 'dangling') + (() if image_only else ('document', 'license')):
                    rejected_case(directory / mode / ('link-' + kind), image_only, link_setup(kind))
                    runs += 1
            def traversal(repo, app):
                return app / '..' / app.name, [], lambda: None
            rejected_case(directory / mode / 'traversal-root', image_only, traversal)
            runs += 1
            if os.name == 'posix':
                for index, name in enumerate(('public\\file', 'public:file', 'public\x01file')):
                    def unsafe_name(repo, app, name=name):
                        write(app / name, b'public malformed path')
                        return app, [], lambda: None
                    rejected_case(directory / mode / f'unsafe-name-{index}', image_only, unsafe_name)
                    runs += 1
            if hasattr(os, 'mkfifo'):
                def fifo(repo, app):
                    os.mkfifo(app / 'public.fifo')
                    return app, [], lambda: None
                rejected_case(directory / mode / 'public-fifo', image_only, fifo)
                runs += 1
        # The existing second image pass must still detect nondeterminism and
        # preserve the first image, rather than publishing a made-up checksum.
        repo, app, _, _, _ = fixture(directory / 'image-determinism-control')
        result = invoke(repo, app, nondeterministic=True)
        assert result.returncode != 0 and 'FFPFSC reproducibility check failed' in result.stderr
        assert len(calls(repo)) == 2
        assert (repo / 'dist' / (RELEASE + '.ffpfsc')).read_bytes().endswith(b'1')
        assert not (repo / 'dist/SHA256SUMS').exists()
        runs += 1
    assert source_bytes == {name: (ROOT / name).read_bytes() for name in SOURCES}, 'Checkout files changed'
    print(f'ENCORE_EXPORT_PRIVACY_PASS CLI_runs={runs} private_probes=0 formats=full,ftp,image,image-only')
    print('Clean ZIP bytes/legal text/root layout and image invocation/determinism controls passed; image is synthetic, NOT PFS.')
    for item in skipped:
        print('SKIP: ' + item)


if __name__ == '__main__':
    main()
