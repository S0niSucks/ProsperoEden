#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real offline CLI tests, synthetic source trees and link materials only. No PS5 build."""
import copy
import ctypes
import ctypes.util
import hashlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest


TOOL = Path(__file__).with_name('source_bundle.py').resolve()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def put(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(value if isinstance(value, bytes) else json.dumps(value).encode())


def git(path, *args):
    return subprocess.check_output(['git', '-C', str(path), *args], stderr=subprocess.DEVNULL).decode().strip()


def init_repo(path):
    path.mkdir()
    git(path, 'init', '-q')
    git(path, 'config', 'user.email', 'synthetic@example.invalid')
    git(path, 'config', 'user.name', 'Synthetic source test')


def commit(path):
    git(path, 'add', '.')
    git(path, 'commit', '-qm', 'synthetic')
    return git(path, 'rev-parse', 'HEAD')


def archive(path, members, links=()):
    with tarfile.open(path, 'w:gz') as output:
        for name, data in members.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            output.addfile(info, io.BytesIO(data))
        for name, target in links:
            info = tarfile.TarInfo(name)
            info.type = tarfile.SYMTYPE
            info.linkname = target
            output.addfile(info)
    return path.read_bytes()


class SourceBundleCLI(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='source-cli-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.repo = self.root / 'app'
        self.materials = self.root / 'inputs'
        self.materials.mkdir()
        init_repo(self.repo)
        dep = self.materials / 'dependency'
        init_repo(dep)
        put(dep / 'library.c', b'int synthetic_library;\n')
        pin = commit(dep)
        self.cpm = {'used': {'repo': 'example/used', 'version': '1', 'hash': ''},
                    'disabled-ci': {'ci': True, 'repo': 'example/ci', 'version': '2'}}
        used = archive(self.materials / 'used.tgz', {'used-1/src.c': b'int used;\n'})
        self.cpm['used']['hash'] = hashlib.sha512(used).hexdigest()
        eden = archive(self.materials / 'eden.tgz', {
            'eden-pin/cpmfile.json': json.dumps(self.cpm).encode(),
            'eden-pin/CMakeLists.txt': b'# synthetic Eden fixture only\n'})
        smb = archive(self.materials / 'smb.tgz', {'smb/COPYING': b'synthetic LGPL notice\n'})
        nfs = archive(self.materials / 'nfs.tgz', {'nfs/COPYING': b'synthetic LGPL/BSD notice\n'})
        put(self.materials / 'header.h', b'// synthetic pinned file\n')
        self.eden_commit = 'a' * 40
        self.deps = {'items': [
            {'name': 'eden', 'kind': 'archive', 'url': 'https://example.invalid/' + self.eden_commit + '.tar.gz',
             'sha256': sha(eden), 'save': '.deps/eden.tgz'},
            {'name': 'libsmb2', 'kind': 'archive', 'url': 'https://example.invalid/smb.tgz', 'sha256': sha(smb)},
            {'name': 'libnfs', 'kind': 'archive', 'url': 'https://example.invalid/nfs.tgz', 'sha256': sha(nfs)},
            {'name': 'headers', 'kind': 'files', 'files': [
                {'name': 'header.h', 'url': 'https://example.invalid/header.h', 'sha256': sha((self.materials / 'header.h').read_bytes())}]},
            {'name': 'dependency', 'kind': 'git', 'url': 'https://example.invalid/dependency.git', 'commit': pin}]}
        self.upstream = {'eden_commit': self.eden_commit, 'archives': {'eden.tgz': sha(eden)}}
        put(self.repo / 'tools/deps.json', self.deps)
        put(self.repo / 'UPSTREAM.json', self.upstream)
        put(self.repo / 'Makefile', b'all:\n\t@echo synthetic-only\n')
        put(self.repo / 'tools/eden-patches/01-test.diff', b'# synthetic patch retained\n')
        put(self.repo / 'headless/overlay.inc', b'// synthetic overlay retained\n')
        put(self.repo / '.gitignore', b'.local/\nconfig/\n*.ignored\n')
        self.refresh_snapshot()
        self.used_key = 'cpm/deps/eden/eden-pin/cpmfile.json/used'
        self.omitted_key = 'cpm/deps/eden/eden-pin/cpmfile.json/disabled-ci'
        self.inventory = {'schema': 1, 'snapshot': self.snap,
                          'materials': {'deps/eden': {'path': 'eden.tgz'}, 'upstream/eden.tgz': {'path': 'eden.tgz'},
                                        'deps/libsmb2': {'path': 'smb.tgz'}, 'deps/libnfs': {'path': 'nfs.tgz'},
                                        'deps/headers/header.h': {'path': 'header.h'},
                                        'deps/dependency': {'path': 'dependency'}, self.used_key: {'path': 'used.tgz'}},
                          'cpm_omissions': {self.omitted_key: {'reason': 'Synthetic CI variant disabled by fixture build.',
                                                             'evidence': 'Makefile'}}}

    def refresh_snapshot(self):
        self.snap = {'commit': commit(self.repo), 'tree': git(self.repo, 'rev-parse', 'HEAD^{tree}')}

    def call(self, command='create', ok=True, name='bundle.tar', inventory=None, extra=()):
        input_path = self.root / 'inventory.json'
        put(input_path, self.inventory if inventory is None else inventory)
        output = self.root / name
        args = [sys.executable, '-B', str(TOOL), command, '--repo', str(self.repo),
                '--commit', self.snap['commit'], '--tree', self.snap['tree'],
                '--inputs', str(input_path), '--materials-root', str(self.materials), '--output', str(output), *extra]
        result = subprocess.run(args, capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0 if ok else 2, result.stderr.decode())
        if not ok:
            self.assertFalse(output.exists(), 'failure published a partial bundle')
            self.assertIn(b'REFUSED:', result.stderr)
        return output, result

    def manifest(self, path):
        with tarfile.open(path) as source:
            return json.load(source.extractfile('SOURCE_BUNDLE.json'))

    def verify(self, bundle, ok=True, checksum=None, snapshot=None):
        snap = snapshot or self.snap
        result = subprocess.run([sys.executable, '-B', str(TOOL), 'verify', '--bundle', str(bundle),
                                 '--sha256', checksum or sha(bundle.read_bytes()),
                                 '--commit', snap['commit'], '--tree', snap['tree']], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 0 if ok else 2, result.stderr.decode())

    def test_reproducible_and_manifest(self):
        first, result = self.call()
        second, _ = self.call(name='second.tar')
        self.assertEqual(first.read_bytes(), second.read_bytes())
        manifest = self.manifest(first)
        self.assertEqual(manifest['snapshot'], self.snap)
        self.assertEqual(manifest['relink']['status'], 'PENDING')
        self.assertIn('PENDING', manifest['source_status'])
        self.assertIn(b'relink=PENDING', result.stdout)
        for name in ('app/Makefile', 'app/headless/overlay.inc', 'app/tools/eden-patches/01-test.diff',
                     'materials/deps/dependency/library.c'):
            self.assertIn(name, manifest['files'])
        self.verify(first)

    def test_plan_discovers_verified_eden_cpm(self):
        path, result = self.call(command='plan', name='plan.json')
        plan = json.loads(path.read_bytes())
        self.assertEqual(plan['requirements'][self.used_key]['hashes']['sha512'], self.cpm['used']['hash'])
        self.assertTrue(plan['requirements'][self.omitted_key]['unhashed'])
        self.assertIn(b'PLAN_ONLY', result.stdout)

    def test_missing_direct_material(self):
        del self.inventory['materials']['deps/libnfs']
        self.call(ok=False)

    def test_missing_file(self):
        (self.materials / 'smb.tgz').unlink()
        self.call(ok=False)

    def test_corrupt_source(self):
        (self.materials / 'smb.tgz').write_bytes(b'corrupted synthetic archive')
        self.call(ok=False)

    def test_corrupt_sha512_cpm(self):
        (self.materials / 'used.tgz').write_bytes(b'changed')
        self.call(ok=False)

    def test_inventory_snapshot_mismatch(self):
        self.inventory['snapshot'] = {**self.snap, 'commit': 'b' * 40}
        self.call(ok=False)

    def test_wrong_declared_tree(self):
        self.call(ok=False, extra=('--tree', 'b' * 40))

    def test_traversal_absolute_and_separator_inputs(self):
        for name in ('../eden.tgz', '/etc/passwd', 'C:/private', 'a\\b', 'a/./b', 'a//b', 'NUL'):
            with self.subTest(name=name):
                self.inventory['materials']['deps/eden']['path'] = name
                self.call(ok=False)

    def test_local_symlink_file_and_parent(self):
        original = self.materials / 'smb.tgz'
        (self.materials / 'link.tgz').symlink_to(original)
        self.inventory['materials']['deps/libsmb2']['path'] = 'link.tgz'
        self.call(ok=False)
        (self.materials / 'dir-link').symlink_to(self.materials, target_is_directory=True)
        self.inventory['materials']['deps/libsmb2']['path'] = 'dir-link/smb.tgz'
        self.call(ok=False)

    def test_ignored_untracked_and_dirty_files_never_read(self):
        sentinel = b'SYNTHETIC-PRIVATE-CANARY-NOT-A-REAL-SECRET'
        put(self.repo / '.local/private', sentinel)
        put(self.repo / 'config/nextendo/oauth.json', sentinel)
        put(self.repo / 'config/network-shares.json.tmp-1', sentinel)
        put(self.repo / 'untracked.txt', sentinel)
        put(self.repo / 'headless/overlay.inc', sentinel)  # modified working file, immutable blob still used
        (self.repo / '.local/link').symlink_to('/nonexistent-private-target')
        path, _ = self.call()
        self.assertNotIn(sentinel, path.read_bytes())
        self.assertEqual(self.manifest(path)['files']['app/headless/overlay.inc']['sha256'], sha(b'// synthetic overlay retained\n'))

    def test_tracked_private_paths_refused_before_blob_export(self):
        for name in ('config/nextendo/oauth.json', 'Config/Nextendo./session', 'CONFIG/network-shares.json.stage', '.local/private'):
            with self.subTest(name=name):
                put(self.repo / name, b'synthetic-denied')
                git(self.repo, 'add', '-f', name)
                git(self.repo, 'commit', '-qm', 'private name fixture')
                self.snap = {'commit': git(self.repo, 'rev-parse', 'HEAD'), 'tree': git(self.repo, 'rev-parse', 'HEAD^{tree}')}
                self.inventory['snapshot'] = self.snap
                self.call(ok=False)
                git(self.repo, 'rm', '-f', name)

    def test_git_symlink_and_case_alias_refused(self):
        (self.repo / 'link').symlink_to('Makefile')
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.call(ok=False)

    def test_case_alias_refused(self):
        put(self.repo / 'makefile', b'case alias')
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.call(ok=False)

    def test_dirty_or_wrong_git_dependency(self):
        put(self.materials / 'dependency/library.c', b'modified')
        self.call(ok=False)
        commit(self.materials / 'dependency')
        self.call(ok=False)

    def test_missing_or_unhashed_cpm_refused(self):
        del self.inventory['cpm_omissions']
        self.call(ok=False)
        self.inventory['materials'][self.omitted_key] = {'path': 'used.tgz'}
        self.call(ok=False)

    def test_unknown_inventory_and_omission(self):
        self.inventory['materials']['unexpected'] = {'path': 'header.h'}
        self.call(ok=False)
        del self.inventory['materials']['unexpected']
        self.inventory['cpm_omissions']['deps/libsmb2'] = {'reason': 'Cannot omit direct pins.', 'evidence': 'Makefile'}
        self.call(ok=False)
        self.inventory['cpm_omissions'] = []
        self.call(ok=False)
        del self.inventory['cpm_omissions']
        self.inventory['extra_sources'] = {}
        self.call(ok=False)

    def test_missing_omission_evidence(self):
        self.inventory['cpm_omissions'][self.omitted_key]['evidence'] = 'absent-file'
        self.call(ok=False)

    def replace_eden(self, members, links=()):
        data = archive(self.materials / 'eden.tgz', members, links)
        self.deps['items'][0]['sha256'] = sha(data)
        self.upstream['archives']['eden.tgz'] = sha(data)
        put(self.repo / 'tools/deps.json', self.deps)
        put(self.repo / 'UPSTREAM.json', self.upstream)
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap

    def test_unsafe_archive_members(self):
        for name in ('../escape', '/escape', 'root/config/Nextendo/oauth.json', 'root/CONFIG/network-shares.json.tmp'):
            with self.subTest(name=name):
                self.replace_eden({name: b'not a real secret'})
                self.call(ok=False)

    def test_escaping_archive_symlink(self):
        self.replace_eden({'root/CMakeLists.txt': b'fixture'}, [('root/link', '../../escape')])
        self.call(ok=False)

    def test_duplicate_archive_path(self):
        self.replace_eden({'root/source.c': b'fixture'}, [('root/source.c', 'elsewhere')])
        self.call(ok=False)

    def test_duplicate_manifest_json_key(self):
        path = self.root / 'duplicate.json'
        path.write_text('{"schema":1,"schema":1}')
        self.call(ok=False, extra=('--inputs', str(path)))

    def test_output_not_overwritten(self):
        output, _ = self.call()
        original = output.read_bytes()
        result = subprocess.run([sys.executable, str(TOOL), 'create', '--repo', str(self.repo),
                                 '--commit', self.snap['commit'], '--tree', self.snap['tree'],
                                 '--inputs', str(self.root / 'inventory.json'), '--materials-root', str(self.materials),
                                 '--output', str(output)], capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(output.read_bytes(), original)

    def test_verify_external_hash_and_snapshot(self):
        bundle, _ = self.call()
        self.verify(bundle, ok=False, checksum='0' * 64)
        self.verify(bundle, ok=False, snapshot={**self.snap, 'tree': '0' * 40})

    def test_verify_tampered_file_even_with_recomputed_tar_hash(self):
        bundle, _ = self.call()
        damaged = self.root / 'damaged.tar'
        with tarfile.open(bundle) as source, tarfile.open(damaged, 'w') as output:
            for info in source:
                data = source.extractfile(info).read()
                if info.name == 'app/Makefile':
                    data = b'TAMPERED'
                    info.size = len(data)
                output.addfile(info, io.BytesIO(data))
        self.verify(damaged, ok=False)

    def add_relink(self):
        binary = b'SYNTHETIC-NOT-A-RELEASE-BINARY'
        put(self.materials / 'final.elf', binary)
        files = []
        for name, role in [('application.o', 'non_lgpl_object'), ('relink.sh', 'commands'),
                           ('ps5-pie.ld', 'link_script'), ('toolchain.txt', 'toolchain_recipe'),
                           ('libsmb2.a', 'lgpl_library'), ('libnfs.a', 'lgpl_library')]:
            data = ('SYNTHETIC ONLY ' + name).encode()
            put(self.materials / name, data)
            files.append({'name': name, 'role': role, 'path': name, 'sha256': sha(data)})
        receipt = {'schema': 1, 'snapshot': self.snap, 'final_binary_sha256': sha(binary),
                   'files': {f['name']: {'sha256': f['sha256'], 'role': f['role']} for f in files},
                   'lgpl_sources': {i['name']: i['sha256'] for i in self.deps['items'] if i['name'] in ('libsmb2', 'libnfs')}}
        put(self.materials / 'receipt.json', receipt)
        self.inventory['relink'] = {'status': 'COLLECT', 'snapshot': self.snap,
                                    'final_binary': {'path': 'final.elf', 'sha256': sha(binary)}, 'files': files,
                                    'receipt': {'path': 'receipt.json', 'sha256': sha((self.materials / 'receipt.json').read_bytes())}}

    def test_collect_relink_never_claims_qualified(self):
        self.add_relink()
        bundle, _ = self.call()
        manifest = self.manifest(bundle)
        self.assertEqual(manifest['relink']['status'], 'COLLECTED_UNVERIFIED_RELINK')
        self.assertNotIn(b'SYNTHETIC-NOT-A-RELEASE-BINARY', bundle.read_bytes())
        self.verify(bundle)

    def test_relink_stale_receipt_missing_objects_corruption(self):
        self.add_relink()
        original = copy.deepcopy(self.inventory)
        self.inventory['relink']['snapshot'] = {**self.snap, 'commit': 'b' * 40}
        self.call(ok=False)
        self.inventory = copy.deepcopy(original)
        self.inventory['relink']['files'] = [f for f in self.inventory['relink']['files'] if f['role'] != 'non_lgpl_object']
        self.call(ok=False)
        self.inventory = copy.deepcopy(original)
        put(self.materials / 'application.o', b'corrupted old object')
        self.call(ok=False)

    def test_relink_old_binary_receipt_cannot_be_renamed_to_release(self):
        self.add_relink()
        receipt = json.loads((self.materials / 'receipt.json').read_bytes())
        receipt['snapshot'] = {**self.snap, 'commit': 'c' * 40}
        put(self.materials / 'receipt.json', receipt)
        self.inventory['relink']['receipt']['sha256'] = sha((self.materials / 'receipt.json').read_bytes())
        self.call(ok=False)

    def test_no_ready_status_or_pending_with_objects(self):
        self.inventory['relink'] = {'status': 'READY'}
        self.call(ok=False)
        self.inventory['relink'] = {'status': 'PENDING', 'files': []}
        self.call(ok=False)

    def test_nested_cpm_dependency_is_required(self):
        nested = {'inner': {'hash': '1' * 128, 'repo': 'example/inner', 'version': '1'}}
        data = archive(self.materials / 'used.tgz', {'used-1/cpmfile.json': json.dumps(nested).encode()})
        self.cpm['used']['hash'] = hashlib.sha512(data).hexdigest()
        self.replace_eden({'eden-pin/cpmfile.json': json.dumps(self.cpm).encode()})
        _, result = self.call(ok=False)
        self.assertIn(b'missing material: cpm/cpm/deps/eden/', result.stderr)

    def test_gitlink_source_cannot_be_silently_skipped(self):
        dependency = self.materials / 'dependency'
        pin = git(dependency, 'rev-parse', 'HEAD')
        git(self.repo, 'update-index', '--add', '--cacheinfo', '160000,' + pin + ',vendor/library')
        git(self.repo, 'commit', '-qm', 'synthetic gitlink')
        self.snap = {'commit': git(self.repo, 'rev-parse', 'HEAD'), 'tree': git(self.repo, 'rev-parse', 'HEAD^{tree}')}
        self.inventory['snapshot'] = self.snap
        self.call(ok=False)
        self.inventory['materials']['gitlink/app/vendor/library'] = {'path': 'dependency'}
        bundle, _ = self.call()
        self.assertIn('materials/gitlink/app/vendor/library/library.c', self.manifest(bundle)['files'])

    def test_private_named_material_rejected_before_lstat(self):
        for name in ('config/nextendo/missing', 'Config/NETWORK-SHARES.JSON.stage', '.local/no-such-file'):
            self.inventory['materials']['deps/eden']['path'] = name
            _, result = self.call(ok=False)
            self.assertIn(b'private or empty path refused', result.stderr)

    def test_local_fifo_refused_without_opening(self):
        os.mkfifo(self.materials / 'pipe')
        self.inventory['materials']['deps/eden']['path'] = 'pipe'
        self.call(ok=False)

    def test_pin_disagreement_refused(self):
        self.upstream['archives']['eden.tgz'] = '0' * 64
        put(self.repo / 'UPSTREAM.json', self.upstream)
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.call(ok=False)

    def test_supplemental_source_is_explicit_hashed_and_not_qualified(self):
        data = archive(self.materials / 'supplement.tgz', {'extra/source.c': b'synthetic source'})
        self.inventory['extra_sources'] = [{'id': 'sdk-source.tar.gz', 'path': 'supplement.tgz',
                                           'sha256': sha(data), 'origin': 'https://example.invalid/source.tgz'}]
        bundle, _ = self.call()
        self.assertEqual(self.manifest(bundle)['materials']['extra/sdk-source.tar.gz']['status'],
                         'OPERATOR_SUPPLIED_SOURCE_REQUIRES_BINARY_MATCH_REVIEW')
        put(self.materials / 'supplement.tgz', b'corrupt')
        self.call(ok=False, name='corrupt.tar')

    def test_verify_duplicate_manifest_refused(self):
        bundle, _ = self.call()
        duplicate = self.root / 'duplicate.tar'
        with tarfile.open(bundle) as source, tarfile.open(duplicate, 'w') as output:
            for info in source:
                data = source.extractfile(info).read()
                output.addfile(info, io.BytesIO(data))
                if info.name == 'SOURCE_BUNDLE.json':
                    output.addfile(info, io.BytesIO(data))
        self.verify(duplicate, ok=False)

    def test_receipt_cannot_hide_additional_fields(self):
        self.add_relink()
        receipt = json.loads((self.materials / 'receipt.json').read_bytes())
        receipt['unreviewed_environment_dump'] = 'synthetic-not-a-real-secret'
        put(self.materials / 'receipt.json', receipt)
        self.inventory['relink']['receipt']['sha256'] = sha((self.materials / 'receipt.json').read_bytes())
        self.call(ok=False)

    def zstd_fixture(self, raw):
        library = ctypes.util.find_library('zstd')
        self.assertIsNotNone(library, 'Tests require existing Docker libzstd for the real Boost archive format')
        zstd = ctypes.CDLL(library)
        zstd.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
        zstd.ZSTD_compressBound.restype = ctypes.c_size_t
        zstd.ZSTD_compress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
        zstd.ZSTD_compress.restype = ctypes.c_size_t
        output = ctypes.create_string_buffer(zstd.ZSTD_compressBound(len(raw)))
        size = zstd.ZSTD_compress(output, len(output), raw, len(raw), 1)
        self.assertLessEqual(size, len(output))
        return output.raw[:size]

    def test_zstd_pinned_source_archive(self):
        # Boost CPM actually uses tar.zst. Exercise this format without installing tools.
        with tarfile.open(self.materials / 'used.tgz') as source:
            data = source.extractfile('used-1/src.c').read() * 100000
        memory = io.BytesIO()
        with tarfile.open(fileobj=memory, mode='w') as output:
            info = tarfile.TarInfo('used-1/src.c')
            info.size = len(data)
            output.addfile(info, io.BytesIO(data))
        frame = self.zstd_fixture(memory.getvalue())
        (self.materials / 'used.tgz').write_bytes(frame)
        self.cpm['used']['hash'] = hashlib.sha512(frame).hexdigest()
        self.replace_eden({'eden-pin/cpmfile.json': json.dumps(self.cpm).encode()})
        bundle, _ = self.call()
        self.verify(bundle)

    def test_truncated_zstd_refused_even_when_pin_matches(self):
        frame = self.zstd_fixture(b'not a complete source archive' * 100)[:-2]
        (self.materials / 'used.tgz').write_bytes(frame)
        self.cpm['used']['hash'] = hashlib.sha512(frame).hexdigest()
        self.replace_eden({'eden-pin/cpmfile.json': json.dumps(self.cpm).encode()})
        self.call(ok=False)


if __name__ == '__main__':
    unittest.main(verbosity=2)
