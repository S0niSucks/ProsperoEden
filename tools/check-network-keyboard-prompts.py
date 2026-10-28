#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Docker-only real backend/private keyboard seam. No UI, emulator, CMake or protocol rebuild."""
import concurrent.futures
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not Path('/.dockerenv').exists():
        raise SystemExit('Docker only')
    work = Path(os.environ.get('NETWORK_PROMPT_WORK', '/work/network-keyboard-prompts'))
    work.mkdir(parents=True, exist_ok=True)
    deps = Path('/work/deps')
    # Reuse only these existing pinned private-volume client archives. No coordinator writes.
    clients = Path(os.environ.get('NETWORK_CLIENT_BUILD', '/work/network-editor-build'))
    archives = [clients / 'smb2/lib/libsmb2.a', clients / 'nfs/lib/libnfs.a']
    if not all(path.is_file() for path in archives):
        raise SystemExit('Existing private-volume SMB/NFS client archives required; no automatic rebuild')
    blocks = re.findall(r'```cpp\n(.*?)```', (ROOT / 'docs/NETWORK_KEYBOARD_PROMPTS.md').read_text(), re.DOTALL)
    assert len(blocks) == 5
    proposal = work / 'documentary-proposal.cpp'
    proposal.write_text('''#include "network/keyboard_prompt.h"
#include "prosperoeden/pe/core/strings.hpp"
#include <future>
using namespace Eden::Network;
using pe::tr;
struct Services {
''' + blocks[0] + '''};
struct EdenServices : Services {
''' + blocks[1] + '''
Editor network_editor_;
EditorResult WithNetworkMutation(const std::function<EditorResult()>& work) { return work(); }
};
''' + blocks[2] + '''
struct Launcher {
Services& services_;
EditorView network_view_;
int network_row_ = 0;
std::atomic<bool> network_stop_{false};
bool network_commit_job_ = false;
std::future<EditorResult> network_job_;
void network_result(EditorResult) {}
void selection(int locations) {
''' + blocks[3] + '''
(void)result;
}
void text_entry(Field field) {
''' + blocks[4] + '''}
};
''')
    subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'headless'),
                    '-fsyntax-only', str(proposal)], check=True)
    print('Documentary service/caller proposal syntax PASS using real tr declarations (isolated facade only, NOT actual UI composition)', flush=True)
    sources = [ROOT / 'headless/network' / name for name in ('transport.cpp', 'network.cpp', 'config.cpp', 'editor.cpp', 'keyboard.cpp')]
    includes = [ROOT / 'headless', deps / 'libsmb2-libsmb2-6.3/include', deps / 'libnfs-libnfs-8.0.0/include']
    for mode in ('normal', 'sanitized'):
        build = work / mode
        build.mkdir(exist_ok=True)
        sanitize = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if mode == 'sanitized' else []
        flags = ['g++', '-std=c++20', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-UNDEBUG', '-pthread',
                 *sanitize, *['-I' + str(path) for path in includes]]

        def compile(source):
            obj = build / (source.stem + '.o')
            subprocess.run([*flags, '-c', str(source), '-o', str(obj)], check=True)
            return obj

        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as workers:
            objects = list(workers.map(compile, sources))
        undefined = subprocess.check_output(['nm', '-C', '-u', *map(str, objects)], text=True)
        assert 'pe::tr' not in undefined and 'pe::Catalog' not in undefined
        env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
        for test in ('keyboard_prompt_check', 'editor_check'):
            obj = compile(ROOT / 'headless/network' / (test + '.cpp'))
            binary = build / test
            subprocess.run(['g++', *sanitize, str(obj), *map(str, objects), *map(str, archives), '-pthread', '-o', str(binary)], check=True)
            subprocess.run([str(binary), tempfile.mkdtemp(prefix=test + '-', dir=build)], check=True, env=env, timeout=45)
        print(mode + ' real keyboard/Editor + existing editor/config regression PASS; no pe::tr/Catalog link or stub', flush=True)
    if sdk := os.environ.get('PS5_PAYLOAD_SDK'):
        compiler = ['clang++-18', '--target=x86_64-sie-ps5', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-isysroot', sdk, '-isystem', sdk + '/target/include/c++/v1', '-isystem', sdk + '/target/include',
                    '-I' + str(ROOT / 'headless'), '-fexceptions', '-fcxx-exceptions']
        for name in ('editor.cpp', 'keyboard.cpp'):
            subprocess.run([*compiler, '-fsyntax-only', str(ROOT / 'headless/network' / name)], check=True)
        subprocess.run([*compiler, '-DPS5_NATIVE=1', '-fsyntax-only', str(ROOT / 'headless/system_keyboard.cpp')], check=True)
        print('SDK changed backend/header + native IME adapter syntax PASS (not executed, no full build)')
    print('SEAM ONLY: actual UI/catalog/service/admission composition PENDING; no real console or NAS qualification')


if __name__ == '__main__':
    main()
