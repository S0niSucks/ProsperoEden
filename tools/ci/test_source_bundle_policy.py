#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit-review positives and adversarial policy/restore tests. No release objects."""
import copy
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import unittest
from unittest.mock import patch

import source_bundle as tool
import test_source_bundle as fixture
import restore_source_policy as trial


class PolicyCLI(unittest.TestCase):
    setUp = fixture.SourceBundleCLI.setUp
    refresh_snapshot = fixture.SourceBundleCLI.refresh_snapshot
    call = fixture.SourceBundleCLI.call
    manifest = fixture.SourceBundleCLI.manifest
    verify = fixture.SourceBundleCLI.verify

    def reviewed_dependency(self, links=None, template=True):
        dep = self.materials / 'dependency'
        fixture.put(dep / 'dir/source.c', b'int source;\n')
        fixture.put(dep / 'run.sh', b'#!/bin/sh\nexit 0\n')
        (dep / 'run.sh').chmod(0o755)
        fixture.put(dep / '.gitattributes', b'library.c export-ignore\nversion.txt export-subst\n')
        fixture.put(dep / 'version.txt', b'$Format:%H$\n')
        if template:
            fixture.put(dep / '.env.example', b'PS5_HOST=192.168.1.100\nFTP_PORT=2121\n')
        for name, text in (links or {}).items():
            path = dep / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.symlink_to(text)
        pin = fixture.commit(dep)
        tree = fixture.git(dep, 'rev-parse', 'HEAD^{tree}')
        self.deps['items'][-1]['commit'] = pin
        fixture.put(self.repo / 'tools/deps.json', self.deps)
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.inventory['source_policy'] = {'git': {'deps/dependency': {
            'commit': pin, 'tree': tree, 'reason': 'Reviewed synthetic public source fixture only.',
            'templates': {'.env.example': {'sha256': fixture.sha((dep / '.env.example').read_bytes()),
                                          'reason': 'Reviewed default-only synthetic public template.'}} if template else {},
            'symlinks': {name: fixture.sha(text.encode()) for name, text in (links or {}).items()}}}, 'archives': {}}
        return dep

    def restore(self, bundle, name='restored', unpack=()):
        output = self.root / name
        result = subprocess.run([sys.executable, '-B', str(fixture.TOOL), 'restore', '--bundle', str(bundle),
                                 '--sha256', fixture.sha(bundle.read_bytes()), '--commit', self.snap['commit'],
                                 '--tree', self.snap['tree'], '--output', str(output),
                                 *[part for key in unpack for part in ('--unpack-material', key)]], capture_output=True, timeout=30)
        self.assertEqual(0, result.returncode, result.stderr.decode())
        return output

    def test_reviewed_template_links_modes_and_attributes_roundtrip(self):
        dep = self.reviewed_dependency({'alias': 'chain', 'chain': 'dir', 'dir/link.c': 'source.c'})
        first, _ = self.call()
        second, _ = self.call(name='second.tar')
        self.assertEqual(first.read_bytes(), second.read_bytes())
        manifest = self.manifest(first)
        self.assertEqual(2, manifest['schema'])
        self.assertEqual(self.inventory['source_policy'], manifest['source_policy'])
        with tarfile.open(first) as archive:
            self.assertTrue(all(member.isfile() for member in archive))
            self.assertNotIn('.env.example', archive.getnames())
        self.verify(first)
        restored = self.restore(first) / 'sources/deps/dependency'
        for name in ('.env.example', 'library.c', 'version.txt', 'dir/source.c', 'run.sh'):
            self.assertEqual((dep / name).read_bytes(), (restored / name).read_bytes())
        self.assertEqual('dir', os.readlink(restored / 'chain'))
        self.assertEqual('chain', os.readlink(restored / 'alias'))
        self.assertTrue((restored / 'run.sh').stat().st_mode & 0o111)

    def test_no_template_or_link_permission_by_default(self):
        self.reviewed_dependency({'link': 'library.c'})
        del self.inventory['source_policy']
        self.call(ok=False)

    def test_wrong_component_commit_tree_or_blob_refused(self):
        self.reviewed_dependency({'link': 'library.c'})
        original = copy.deepcopy(self.inventory)
        for field in ('commit', 'tree'):
            self.inventory = copy.deepcopy(original)
            self.inventory['source_policy']['git']['deps/dependency'][field] = '0' * 40
            self.call(ok=False)
        for group, name in [('templates', '.env.example'), ('symlinks', 'link')]:
            self.inventory = copy.deepcopy(original)
            value = self.inventory['source_policy']['git']['deps/dependency'][group]
            if group == 'templates':
                value[name]['sha256'] = '0' * 64
            else:
                value[name] = '0' * 64
            self.call(ok=False)
        self.inventory = copy.deepcopy(original)
        self.inventory['source_policy']['git']['deps/another'] = self.inventory['source_policy']['git'].pop('deps/dependency')
        self.call(ok=False)

    def test_no_other_private_names_or_template_aliases_can_be_reviewed(self):
        self.reviewed_dependency()
        original = copy.deepcopy(self.inventory)
        for name in ('.env', '.ENV.EXAMPLE', 'folder/.env.example', '.env.example.', 'oauth.json',
                     'config/nextendo/client-token', 'config/network-shares.json', 'prod.keys'):
            self.inventory = copy.deepcopy(original)
            review = self.inventory['source_policy']['git']['deps/dependency']
            review['templates'] = {name: next(iter(review['templates'].values()))}
            self.call(ok=False)

    def test_app_template_policy_cannot_bypass_privacy(self):
        fixture.put(self.repo / '.env.example', b'synthetic private app data')
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.inventory['source_policy'] = {'git': {'app': {'commit': self.snap['commit'], 'tree': self.snap['tree'],
            'reason': 'Not a permitted app exception.', 'templates': {}, 'symlinks': {}}}, 'archives': {}}
        self.call(ok=False)

    def test_private_dependency_denied_before_private_metadata_or_blob(self):
        dep = self.reviewed_dependency()
        fixture.put(dep / 'config/nextendo/client-token', b'PRIVATE-CANARY')
        pin = fixture.commit(dep)
        review = self.inventory['source_policy']['git']['deps/dependency']
        review['commit'], review['tree'] = pin, fixture.git(dep, 'rev-parse', 'HEAD^{tree}')
        collector = tool.Collector(self.root / 'stage', self.materials, self.inventory)
        original_git, original_lstat = tool.git, Path.lstat
        def guarded_git(repo, *args):
            if args[:2] == ('cat-file', 'blob'):
                self.fail('private preflight must precede all dependency blob reads')
            return original_git(repo, *args)
        def guarded_lstat(path, *args, **kwargs):
            if 'nextendo' in str(path).lower() or str(path).endswith('.env.example'):
                self.fail('private preflight must precede dependency child metadata')
            return original_lstat(path, *args, **kwargs)
        with patch.object(tool, 'git', guarded_git), patch.object(Path, 'lstat', guarded_lstat):
            with self.assertRaises(tool.Refused):
                collector.collect('deps/dependency', {'kind': 'git', 'commit': pin})

    def test_wrong_template_hash_denied_before_local_template_metadata(self):
        dep = self.reviewed_dependency()
        review = self.inventory['source_policy']['git']['deps/dependency']
        review['templates']['.env.example']['sha256'] = '0' * 64
        collector = tool.Collector(self.root / 'stage', self.materials, self.inventory)
        original = Path.lstat
        def guarded(path, *args, **kwargs):
            if path.name == '.env.example':
                self.fail('wrong immutable review hash must precede local template metadata')
            return original(path, *args, **kwargs)
        with patch.object(Path, 'lstat', guarded), self.assertRaises(tool.Refused):
            collector.collect('deps/dependency', {'kind': 'git', 'commit': review['commit']})

    def test_dirty_link_text_and_executable_mode_refused(self):
        dep = self.reviewed_dependency({'link': 'library.c'})
        (dep / 'link').unlink()
        (dep / 'link').symlink_to('/not-a-readable-private-target')
        self.call(ok=False)
        (dep / 'link').unlink()
        (dep / 'link').symlink_to('library.c')
        (dep / 'run.sh').chmod(0o644)
        self.call(ok=False)

    def test_unsafe_link_targets_even_with_matching_review(self):
        for text in ('/absolute', 'C:/windows', '\\server\\share', '../escape', 'dir/../../escape',
                     'NUL', '.env.example', 'config/nextendo/../client', 'DIR/source.c', 'absent'):
            with self.subTest(text=text):
                # Independent fixture per target, all public/synthetic; explicit review
                # is deliberately insufficient to authorize an unsafe graph.
                with self.subTest():
                    temp = PolicyCLI('test_no_template_or_link_permission_by_default')
                    temp.setUp()
                    try:
                        temp.reviewed_dependency({'link': text})
                        temp.call(ok=False)
                    finally:
                        temp.doCleanups()

    def test_graph_cycles_aliases_and_link_prefixes(self):
        cases = [
            {'a': {'kind': 'symlink', 'target': 'b'}, 'b': {'kind': 'symlink', 'target': 'a'}},
            {'d/file': {'kind': 'file'}, 'd/back': {'kind': 'symlink', 'target': '../d'}},
            {'a': {'kind': 'symlink', 'target': 'd'}, 'd/back': {'kind': 'symlink', 'target': '../a'}},
            {'Foo/a': {'kind': 'file'}, 'foo/b': {'kind': 'file'}},
            {'link': {'kind': 'symlink', 'target': 'd'}, 'link/child': {'kind': 'file'}, 'd/f': {'kind': 'file'}},
            {'f': {'kind': 'file'}, 'link': {'kind': 'symlink', 'target': 'f/../f'}},
        ]
        for case in cases:
            with self.subTest(case=case), self.assertRaises(tool.Refused):
                tool.link_graph(case)

    def test_windows_device_and_wildcard_aliases_refused(self):
        for name in ('CONIN$', 'conout$.txt', 'COM¹', 'LPT².log', 'CLOCK$', 'file?', 'a|b', 'a<b'):
            with self.subTest(name=name), self.assertRaises(tool.Refused):
                tool.safe_name(name)

    def test_safe_redundant_separator_in_relative_link_text_preserved(self):
        self.reviewed_dependency({'link': './dir//source.c'})
        bundle, _ = self.call()
        self.verify(bundle)
        restored = self.restore(bundle) / 'sources/deps/dependency'
        self.assertEqual('./dir//source.c', os.readlink(restored / 'link'))
        self.assertEqual(b'int source;\n', (restored / 'link').read_bytes())

    def hardlink_eden(self, extra=None, target='root/gunzip', before=False):
        for table in ('materials', 'cpm_omissions'):
            self.inventory[table] = {key.replace('/eden-pin/cpmfile.json/', '/root/cpmfile.json/'): value
                                     for key, value in self.inventory[table].items()}
        rows = [('root/cpmfile.json', 'file', json.dumps(self.cpm).encode()), ('root/CMakeLists.txt', 'file', b'fixture')]
        link = ('root/gzip', 'hardlink', target)
        regular = ('root/gunzip', 'file', b'synthetic decompressor source')
        rows.extend([link, regular] if before else [regular, link])
        rows.extend(extra or [])
        with tarfile.open(self.materials / 'eden.tgz', 'w:gz') as archive:
            for name, kind, value in rows:
                info = tarfile.TarInfo(name)
                if kind == 'file':
                    info.size = len(value)
                    archive.addfile(info, io.BytesIO(value))
                else:
                    info.type = {'hardlink': tarfile.LNKTYPE, 'symlink': tarfile.SYMTYPE, 'fifo': tarfile.FIFOTYPE}[kind]
                    info.linkname = value
                    archive.addfile(info)
        checksum = fixture.sha((self.materials / 'eden.tgz').read_bytes())
        self.deps['items'][0]['sha256'] = checksum
        self.upstream['archives']['eden.tgz'] = checksum
        fixture.put(self.repo / 'tools/deps.json', self.deps)
        fixture.put(self.repo / 'UPSTREAM.json', self.upstream)
        self.refresh_snapshot()
        self.inventory['snapshot'] = self.snap
        self.inventory['source_policy'] = {'git': {}, 'archives': {'deps/eden': {
            'sha256': checksum, 'reason': 'Reviewed synthetic pinned archive hardlink only.',
            'hardlinks': {'root/gzip': {'target': target, 'reason': 'Exact reviewed synthetic root-relative regular target.'}}}}}

    def test_reviewed_hardlink_both_member_orders_and_safe_restore(self):
        for before in (False, True):
            self.hardlink_eden(before=before)
            bundle, _ = self.call(name=f'bundle-{before}.tar')
            self.verify(bundle)
            restored = self.restore(bundle, name=f'restored-{before}', unpack=['deps/eden']) / 'unpacked/deps/eden/root'
            self.assertEqual((restored / 'gunzip').stat().st_ino, (restored / 'gzip').stat().st_ino)

    def test_hardlink_not_a_default_permission(self):
        self.hardlink_eden()
        del self.inventory['source_policy']
        self.call(ok=False)

    def test_hardlink_wrong_hash_target_and_unused_policy(self):
        self.hardlink_eden()
        original = copy.deepcopy(self.inventory)
        self.inventory['source_policy']['archives']['deps/eden']['sha256'] = '0' * 64
        self.call(ok=False)
        self.inventory = copy.deepcopy(original)
        self.inventory['source_policy']['archives']['deps/eden']['hardlinks']['root/gzip']['target'] = 'root/CMakeLists.txt'
        self.call(ok=False)
        self.inventory = copy.deepcopy(original)
        self.inventory['source_policy']['archives']['unused'] = self.inventory['source_policy']['archives'].pop('deps/eden')
        self.call(ok=False)

    def test_hardlink_chains_cycles_aliases_links_fifo_refused(self):
        for target in ('root/gzip', 'root/absent', '../escape', '/absolute', 'root/GUNZIP', 'root/alias'):
            self.hardlink_eden([('root/alias', 'symlink', 'gunzip')], target=target)
            self.call(ok=False)
        for extra in ([('root/other', 'hardlink', 'root/gzip')], [('root/pipe', 'fifo', '')],
                      [('root/gunzip/child', 'file', b'prefix collision')]):
            self.hardlink_eden(extra)
            self.call(ok=False)

    def test_archive_audit_never_extracts(self):
        self.hardlink_eden()
        review = self.inventory['source_policy']['archives']['deps/eden']
        with patch.object(Path, 'mkdir', side_effect=AssertionError('audit must not extract')):
            tool.archive_index(self.materials / 'eden.tgz', review)

    def repack(self, bundle, change):
        result = self.root / 'tampered.tar'
        with tarfile.open(bundle) as archive:
            payloads = {i.name: (copy.copy(i), archive.extractfile(i).read()) for i in archive}
        manifest = json.loads(payloads['SOURCE_BUNDLE.json'][1])
        change(manifest, payloads)
        info = payloads['SOURCE_BUNDLE.json'][0]
        data = tool.canonical(manifest)
        info.size = len(data)
        payloads['SOURCE_BUNDLE.json'] = (info, data)
        with tarfile.open(result, 'w') as output:
            for info, data in payloads.values():
                output.addfile(info, io.BytesIO(data))
        return result

    def test_verify_cannot_omit_git_blob_or_change_source_mode(self):
        self.reviewed_dependency({'link': 'library.c'})
        bundle, _ = self.call()
        for kind in ('omit', 'mode', 'proof'):
            def change(manifest, payloads):
                proof = manifest['git_sources']['deps/dependency']
                if kind == 'omit':
                    del proof['entries']['library.c']
                elif kind == 'mode':
                    proof['entries']['run.sh']['mode'] = '100644'
                else:
                    del manifest['git_sources']['deps/dependency']
            self.verify(self.repack(bundle, change), ok=False)

    def test_extra_git_pins_and_nested_cpm_gitlinks_are_not_skipped(self):
        extra = self.materials / 'extra'
        fixture.init_repo(extra)
        fixture.put(extra / 'cpmfile.json', json.dumps({'pkg': self.cpm['used']}).encode())
        child_pin = self.deps['items'][-1]['commit']
        fixture.git(extra, 'add', '.')
        fixture.git(extra, 'update-index', '--add', '--cacheinfo', '160000,' + child_pin + ',vendor/child')
        fixture.git(extra, 'commit', '-qm', 'supplement')
        self.inventory['extra_git_sources'] = [{'id': 'sdk', 'path': 'extra', 'commit': fixture.git(extra, 'rev-parse', 'HEAD'),
            'tree': fixture.git(extra, 'rev-parse', 'HEAD^{tree}'), 'origin': 'https://example.invalid/sdk.git'}]
        self.call(ok=False)
        self.inventory['materials']['gitlink/extra-git/sdk/vendor/child'] = {'path': 'dependency'}
        self.inventory['materials']['cpm/extra-git/sdk/cpmfile.json/pkg'] = {'path': 'used.tgz'}
        bundle, _ = self.call(name='complete.tar')
        self.verify(bundle)
        restored = self.restore(bundle)
        self.assertTrue((restored / 'sources/extra-git/sdk/vendor/child/library.c').is_file())
        self.inventory['extra_git_sources'][0]['tree'] = '0' * 40
        self.call(ok=False, name='wrong.tar')

    def test_extra_archive_nested_cpm_is_discovered_and_required(self):
        source = self.materials / 'supplement.tgz'
        fixture.archive(source, {'root/cpmfile.json': json.dumps({'nested': self.cpm['used']}).encode()})
        self.inventory['extra_sources'] = [{'id': 'recipe.tgz', 'path': 'supplement.tgz',
            'sha256': fixture.sha(source.read_bytes()), 'origin': 'https://example.invalid/source.tgz'}]
        self.call(ok=False)
        key = 'cpm/extra/recipe.tgz/root/cpmfile.json/nested'
        self.inventory['materials'][key] = {'path': 'used.tgz'}
        bundle, _ = self.call(name='complete.tar')
        self.verify(bundle)
        self.assertIn(key, self.manifest(bundle)['requirements'])

    def test_verify_rediscovery_rejects_invented_or_removed_cpm_requirement(self):
        bundle, _ = self.call()
        def change(manifest, payloads):
            del manifest['requirements']['cpm/deps/eden/eden-pin/cpmfile.json/used']
        self.verify(self.repack(bundle, change), ok=False)

    def test_restored_inventory_reads_link_text_without_following_target(self):
        source = self.root / 'restored-fixture'
        source.mkdir()
        (source / 'link').symlink_to('/unreadable-private-fixture')
        fixture.put(source / 'regular', b'public restored source')
        report = trial.source_inventory(source)
        self.assertEqual({'link_text': '/unreadable-private-fixture'}, report['link'])
        self.assertEqual(fixture.sha(b'public restored source'), report['regular']['sha256'])

    def test_source_patch_trial_checks_forward_apply_and_reverse(self):
        source = self.root / 'source-fixture'
        source.mkdir()
        fixture.put(source / 'text', b'before\n')
        patch = self.root / 'source.diff'
        fixture.put(patch, b'--- a/text\n+++ b/text\n@@ -1 +1 @@\n-before\n+after\n')
        trial.patch_source(source, patch)
        self.assertEqual(b'after\n', (source / 'text').read_bytes())
        with self.assertRaises(subprocess.CalledProcessError):
            trial.patch_source(source, patch)


if __name__ == '__main__':
    unittest.main(verbosity=2)
