#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused network admission/controller fixtures. Docker, immutable /src, exec tmpfs only."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def definition(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\(', text, re.M)
    assert match, name
    start = text.index('{', match.start())
    depth = 0
    for token in re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[start:], re.S):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return text[match.start():start + token.end()]
    raise AssertionError(name)


def main():
    if not Path('/.dockerenv').exists(): raise SystemExit('Docker only')
    source = (ROOT / 'headless/prosperoeden/eden_services.cpp').read_text()
    ui = (ROOT / 'headless/prosperoeden/pe/ui/network_shares.cpp').read_text()
    with tempfile.TemporaryDirectory(prefix='network-compose-') as temporary:
        folder = Path(temporary)
        (folder / 'prosperoeden').mkdir()
        header = (ROOT / 'headless/prosperoeden/eden_services.h').read_text()
        header = header.replace('class EdenServices final : public pe::ui::Services', 'class EdenServices')
        header = header.replace(' override', '').replace('private:', 'public:')
        (folder / 'prosperoeden/eden_services.h').write_text(header)
        methods = ('can_launch_game', 'set_games_folder', 'set_files_folder', 'profiles', 'profile_key',
                   'choose_profile', 'add_profile', 'rename_profile', 'remove_profile', 'will_play')
        (folder / 'compose_service_methods.inc').write_text('\n'.join(definition(source, 'EdenServices::' + name) for name in methods))
        methods = ('network_result', 'open_network_shares', 'start_network_job', 'update_network_shares', 'press_network_shares')
        (folder / 'compose_ui_methods.inc').write_text('\n'.join(definition(ui, 'Launcher::' + name) for name in methods))
        (folder / 'compose_ui_saved.inc').write_text(definition(ui, 'Saved'))
        assert 'PrepareKeyboardPrompts' in ui and 'network_mutation_mutex_' not in header
        deps = Path(os.environ.get('NETWORK_DEPS_VOLUME', '/dependencies'))
        sources = [ROOT / 'headless' / name for name in ('network/compose_check.cpp', 'network/editor.cpp', 'network/config.cpp',
                   'network/keyboard.cpp', 'network/transport.cpp', 'usb_backup.cpp', 'cheat_manager.cpp', 'cheat_store.cpp',
                   'cheat_catalog.cpp', 'cheat_download.cpp', 'amiibo_sync.cpp', 'amiibo_sync_format.cpp',
                   'nextendo.cpp', 'nextendo_http.cpp', 'nextendo_oauth.cpp', 'prosperoeden/pe/core/strings.cpp')]
        includes = [folder, ROOT / 'headless', ROOT / 'headless/prosperoeden', ROOT / 'third_party',
                    deps / 'deps/libsmb2-libsmb2-6.3/include', deps / 'deps/libnfs-libnfs-8.0.0/include']
        archives = [deps / 'network-editor-build/smb2/lib/libsmb2.a', deps / 'network-editor-build/nfs/lib/libnfs.a']
        subprocess.run(['clang++-18', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                        *['-I' + str(p) for p in includes], '-fsyntax-only', str(sources[0])], check=True)
        for mode, sanitizers in [('normal', []), ('asan-ubsan', ['-fsanitize=address,undefined']), ('tsan', ['-fsanitize=thread'])]:
            binary = folder / ('check-' + mode)
            subprocess.run(['clang++-18', '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
                            *sanitizers, '-fno-omit-frame-pointer', *['-I' + str(p) for p in includes],
                            *map(str, sources), *map(str, archives), '-lssl', '-lcrypto', '-lz', '-ldl', '-pthread', '-o', str(binary)], check=True)
            env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1',
                   'TSAN_OPTIONS': 'halt_on_error=1'}
            subprocess.run([str(binary)], check=True, env=env, timeout=75)
            print('Network composition variant PASS: ' + mode, flush=True)


if __name__ == '__main__': main()
