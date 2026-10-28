#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline preparation tests only; the reviewed source_bundle tests remain untouched."""
import hashlib
import os
from pathlib import Path
import tempfile
import unittest

import gather_release_sources as gather


class PreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'explicit-cache.archive'
        self.source.write_bytes(b'public synthetic source bytes\n')
        self.expected = {'sha256': hashlib.sha256(self.source.read_bytes()).hexdigest()}
        self.records, self.blockers = {}, {}

    def acquire(self, **kwargs):
        arguments = dict(root=self.root, name='archives/source.archive', hashes=self.expected,
                         candidates=[self.source], urls=[], download=False,
                         records=self.records, blockers=self.blockers)
        arguments.update(kwargs)
        return gather.acquire(**arguments)

    def test_exact_hash_copy(self):
        target = self.acquire()
        self.assertEqual(self.source.read_bytes(), target.read_bytes())
        self.assertEqual(self.records['archives/source.archive']['sha256'], self.expected['sha256'])
        self.assertFalse(self.blockers)

    def test_repeat_records_are_identical(self):
        self.acquire()
        first = gather.canonical(self.records)
        self.acquire()
        self.assertEqual(first, gather.canonical(self.records))

    def test_bad_cache_does_not_become_material(self):
        self.assertIsNone(self.acquire(hashes={'sha256': '0' * 64}))
        self.assertFalse(self.records)
        self.assertFalse((self.root / 'materials/archives/source.archive').exists())
        self.assertEqual('HASH_MISMATCH', self.blockers['archives/source.archive']['attempts'][0]['result'])

    def test_corrupt_private_input_is_not_replaced(self):
        target = self.acquire()
        target.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'existing private material changed'):
            self.acquire()
        self.assertEqual(b'changed', target.read_bytes())

    def test_no_implicit_download(self):
        self.assertIsNone(self.acquire(candidates=[], urls=['https://example.invalid/source']))
        self.assertEqual([], self.blockers['archives/source.archive']['attempts'])

    def test_unsafe_material_paths(self):
        for name in ('', '/absolute', '../escape', 'a/../escape', 'a//b', 'a\\b', 'C:escape'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.acquire(name=name)

    def test_supported_hashes_are_required(self):
        for hashes in ({}, {'md5': '0' * 32}, {'sha256': 'abcd'}, {'sha512': 'x' * 128}):
            with self.subTest(hashes=hashes), self.assertRaises(ValueError):
                self.acquire(hashes=hashes)

    def test_symlink_cache_refused(self):
        path = self.root / 'cache-link'
        path.symlink_to(self.source)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.acquire(candidates=[path])

    def test_fifo_cache_refused_without_open(self):
        path = self.root / 'cache-fifo'
        os.mkfifo(path)
        with self.assertRaisesRegex(ValueError, 'non-regular'):
            self.acquire(candidates=[path])

    def test_private_network_paths_refused_before_stat(self):
        for name in ('config/nextendo/token.json', 'config/network-shares.json', 'CONFIG/NEXTENDO/key'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'before stat'):
                gather.regular(self.root / name)

    def test_symlink_output_parent_refused(self):
        (self.root / 'materials').symlink_to(self.root)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.acquire()
        self.assertFalse((self.root / 'archives').exists())

    def test_stale_partial_is_not_discarded(self):
        partial = self.root / 'materials/archives/source.archive.partial'
        partial.parent.mkdir(parents=True)
        partial.write_bytes(b'other work')
        with self.assertRaisesRegex(ValueError, 'partial'):
            self.acquire()
        self.assertEqual(b'other work', partial.read_bytes())

    def test_public_urls_require_no_credentials(self):
        for url in ('http://example.com/a', 'https://a:b@example.com/a', 'https://example.com/a?token=x',
                    'https://example.com/a#secret', 'file:///etc/passwd'):
            with self.subTest(url=url), self.assertRaises(ValueError):
                gather.public_url(url)
        self.assertEqual('https://example.com/a', gather.public_url('https://example.com/a'))

    def test_cpm_exact_archive_url(self):
        self.assertEqual('https://github.com/owner/repo/archive/v1.2.3.tar.gz',
                         gather.cpm_url({'repo': 'owner/repo', 'version': 'v1.2.3'}))

    def test_cpm_exact_release_artifact(self):
        self.assertEqual('https://github.com/owner/repo/releases/download/v1.2.3/pkg-1.2.3.tar.zst',
                         gather.cpm_url({'repo': 'owner/repo', 'version': 'v1.2.3', 'numeric_version': '1.2.3',
                                         'artifact': 'pkg-%NUMERIC_VERSION%.tar.zst'}))

    def test_github_alternate_does_not_repin(self):
        self.assertEqual('https://codeload.github.com/owner/repo/tar.gz/refs/tags/v1.2.3',
                         gather.alternate('https://github.com/owner/repo/archive/refs/tags/v1.2.3.tar.gz'))
        self.assertIsNone(gather.alternate('https://github.com/owner/repo/releases/download/v1.2.3/pkg.tar.gz'))

    def test_real_local_git_pin_and_dirty_refusal(self):
        source = self.root / 'public-git'
        gather.command(['git', 'init', '-q', str(source)])
        (source / 'source.c').write_text('int answer = 42;\n')
        gather.git(source, 'add', 'source.c')
        gather.git(source, '-c', 'user.name=Synthetic', '-c', 'user.email=synthetic@example.invalid', 'commit', '-qm', 'synthetic')
        commit = gather.git(source, 'rev-parse', 'HEAD').decode().strip()
        dep = {'url': 'https://example.invalid/public.git', 'commit': commit}
        target = gather.clone_pin(self.root, 'synthetic', dep, source, False, self.records, self.blockers)
        self.assertEqual(commit, self.records['git/synthetic']['commit'])
        (target / 'source.c').write_text('changed\n')
        self.assertIsNone(gather.clone_pin(self.root, 'synthetic', dep, source, False, self.records, self.blockers))
        self.assertNotIn('git/synthetic', self.records)
        self.assertIn('git/synthetic', self.blockers)


if __name__ == '__main__':
    unittest.main()
