#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual service admission methods + real workers; synthetic paths/probes/transport. Docker only."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/prosperoeden/eden_services.cpp').read_text()
header = (root / 'headless/prosperoeden/eden_services.h').read_text()
methods = ('bool EdenServices::can_launch_game()', 'bool EdenServices::set_files_folder(',
           'bool EdenServices::set_games_folder(', 'bool EdenServices::check_cheat_database(',
           'bool EdenServices::install_cheat_database(', 'bool EdenServices::start_amiibo_sync(',
           'std::vector<pe::ui::Profile> EdenServices::profiles()', 'std::string EdenServices::profile_key(',
           'bool EdenServices::choose_profile(', 'int EdenServices::add_profile(',
           'bool EdenServices::rename_profile(', 'bool EdenServices::remove_profile(',
           'void EdenServices::will_play(')
with tempfile.TemporaryDirectory(prefix='launcher-admission-') as tmp:
    folder = Path(tmp)
    # Compile verbatim production method definitions and the actual guard/worker member block.
    # Only unrelated console/core services and the fixture's surrounding class are simulated.
    (folder / 'launcher_admission_methods.inc').write_text('\n'.join(
        signature + source.split(signature, 1)[1].split('\n}', 1)[0] + '\n}' for signature in methods))
    (folder / 'launcher_admission_members.inc').write_text(
        'std::mutex bridge_;' + header.split('std::mutex bridge_;', 1)[1].split(
            'Eden::CheatManagerRequest CheatRequest', 1)[0])
    for flags in ([], ['-fsanitize=thread', '-fno-omit-frame-pointer'],
                  ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        subprocess.run(['clang++-18', '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
                        *flags, '-I', str(folder), '-I', str(root / 'headless'),
                        *[str(root / 'headless' / name) for name in ('launcher_admission_check.cpp',
                          'cheat_manager.cpp', 'cheat_store.cpp', 'cheat_catalog.cpp', 'cheat_download.cpp',
                          'amiibo_sync.cpp', 'amiibo_sync_format.cpp', 'usb_backup.cpp',
                          'nextendo.cpp', 'nextendo_http.cpp', 'nextendo_oauth.cpp')],
                        '-lssl', '-lcrypto', '-lz', '-pthread', '-o', str(folder / 'check')], check=True)
        subprocess.run([str(folder / 'check')], check=True, timeout=60)
