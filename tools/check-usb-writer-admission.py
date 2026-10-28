#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verbatim service/worker bodies, real settings/mod/profile files; synthetic console/backend I/O.

Docker only. --baseline <saved eden_services.cpp> proves the old late-bridge writer on real files.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline', type=Path)
parser.add_argument('--remote-baseline', type=Path)
args = parser.parse_args()
source = (args.baseline or root / 'headless/prosperoeden/eden_services.cpp').read_text()
header = (root / 'headless/prosperoeden/eden_services.h').read_text()


def definition(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\(', text, re.M)
    assert match, name
    start = text.index('{', match.start())
    depth = 0
    # Ignore braces in comments and strings; return the exact source, not a rewritten guard.
    for token in re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[start:], re.S):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth:
                return text[match.start():start + token.end()]
    raise AssertionError(name)


methods = '''set_docked docked set_game_settings game_settings preferences set_preferences
room_connection connect_room connect_public_room remember_room saved_files_folder saved_games_folder
post_processing_shared post_processing_labels ReadPostPresets PostPresetIndex PostPresetName
mods cheats set_cheat_enabled cheats_enabled set_cheats_enabled set_mod_enabled mods_enabled
set_mods_enabled make_mods_folder ModPlaces refresh_sources download cancel_download delete_game Titles
RefreshTitles start_update apply_update save_import save_export save_sync_wanted start_save_sync
StartSync save_sync choose_save_data end_save_sync stop_save_sync will_play save_sync_setup
start_pairing pairing cancel_pairing unlink_profile'''.split()
if 'bool EdenServices::SaveSyncWanted(' in source:
    methods.append('SaveSyncWanted')

with tempfile.TemporaryDirectory(prefix='usb-writers-') as tmp:
    folder = Path(tmp)
    # Real member declarations, no vtable for unrelated console-only functions (not an ABI test).
    # ObservedMutex only pauses at acquisition in the test; it derives from std::mutex so the
    # real CheatManager retains its std::mutex collaborator. No production method is instrumented.
    fixture = header.replace('class EdenServices final : public pe::ui::Services', 'class EdenServices')
    fixture = fixture.replace(' override', '').replace('private:', 'public:')
    fixture = fixture.replace('std::mutex bridge_;', 'ObservedMutex bridge_;')
    fixture = fixture.replace('std::mutex job_admission_mutex_;', 'ObservedMutex job_admission_mutex_;')
    fixture = fixture.replace('std::atomic<bool> usb_active_{false}, usb_stop_{false};',
                              'ObservedAtomic usb_active_{false}; std::atomic<bool> usb_stop_{false};')
    (folder / 'usb_writer_class.h').write_text(fixture)
    # SaveSyncWanted now recognizes network identities. Keep the actual production predicate,
    # not a weakened stub; no SMB/NFS transport or dependency rebuild is needed for this seam.
    network = (root / 'headless/network/transport.cpp').read_text()
    (folder / 'usb_writer_methods.inc').write_text(
        'namespace Eden::Network {\n' + definition(network, 'IsPath') + '\n}\n' +
        '\n\n'.join(definition(source, 'EdenServices::' + name) for name in methods))
    # The helper block contains actual pending JSON, profile reconciliation, SyncSetupOf and pairing sources.
    (folder / 'usb_writer_helpers.inc').write_text(
        'void KeepSaveSyncFile(' + source.split('void KeepSaveSyncFile(', 1)[1].split('} // namespace', 1)[0])
    (folder / 'app_version.h').write_text('#define EDEN_APP_VERSION "synthetic-usb-audit"\n')
    variants = [('', [])] if args.baseline else [
        ('normal', []), ('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']),
        ('tsan', ['-fsanitize=thread', '-fno-omit-frame-pointer'])]
    for label, flags in variants:
        binary = folder / 'service-check'
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        '-Wall', '-Wextra', '-Werror', '-DEDEN_SAVE_IMPORT', *flags, *(['-DEXPECT_BASE_GAP'] if args.baseline else []),
                        '-I', str(folder), '-I', str(root / 'headless'), '-I', str(root / 'headless/prosperoeden'),
                        '-I', str(root / 'third_party'), str(root / 'headless/usb_writer_admission_check.cpp'),
                        str(root / 'headless/update_notice.cpp'), str(root / 'headless/usb_backup.cpp'),
                        str(root / 'headless/cheat_manager.cpp'), str(root / 'headless/cheat_store.cpp'),
                        str(root / 'headless/cheat_catalog.cpp'), str(root / 'headless/cheat_download.cpp'),
                        str(root / 'headless/amiibo_sync.cpp'), str(root / 'headless/amiibo_sync_format.cpp'),
                        str(root / 'headless/prosperoeden/pe/core/strings.cpp'),
                        '-lcrypto', '-lz', '-pthread', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=90)
        print('USB writer service variant:', label or 'exact-base-gap-proof', flush=True)
    remote = (args.remote_baseline or root / 'headless/remote/remote.cpp').read_text()
    (folder / 'remote_worker_source.inc').write_text(remote)
    for label, flags in variants:
        binary = folder / 'remote-check'
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        '-Wall', '-Wextra', '-Werror', *flags, *(['-DEXPECT_BASE_GAP'] if args.remote_baseline else []),
                        '-I', str(folder), '-I', str(root / 'headless'), '-I', str(root / 'headless/remote'),
                        '-I', str(root / 'third_party'), str(root / 'headless/remote_snapshot_check.cpp'),
                        str(root / 'headless/remote/ftp.cpp'), str(root / 'headless/remote/stream_check.cpp'),
                        '-Wl,--wrap=remove', '-lcrypto', '-pthread', '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True, timeout=40)
        print('USB remote worker variant:', label or 'exact-base-gap-proof', flush=True)
