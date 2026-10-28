#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real service admission/Nextendo composition, synthetic provider only. Docker/private scratch."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='NEW private output directory')
    args = parser.parse_args()
    assert Path('/.dockerenv').exists(), 'Docker only'
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    source_path = root / 'headless/prosperoeden/eden_services.cpp'
    header_path = root / 'headless/prosperoeden/eden_services.h'
    fixture_path = root / 'headless/launcher_admission_check.cpp'
    source, header, fixture = (p.read_text() for p in (source_path, header_path, fixture_path))
    # Neutral metadata missing from the older shared fixture, not a production guard change.
    # Keep these fixture-only additions here: network ownership is held by another worker.
    assert fixture.count('bool remote = false;') == 1
    assert fixture.count('class EdenServices {\npublic:\n') == 1
    fixture = fixture.replace('bool remote = false;', 'bool remote = false; bool network = false;', 1)
    fixture = fixture.replace('class EdenServices {\npublic:\n',
        'class EdenServices {\npublic:\n    bool network_mutation_active_ = false; // neutral fixture metadata only\n', 1)
    (out / 'composed_check.cpp').write_text(fixture)
    methods = ('bool EdenServices::can_launch_game()', 'bool EdenServices::set_files_folder(',
               'bool EdenServices::set_games_folder(', 'bool EdenServices::check_cheat_database(',
               'bool EdenServices::install_cheat_database(', 'bool EdenServices::start_amiibo_sync(',
               'std::vector<pe::ui::Profile> EdenServices::profiles()', 'std::string EdenServices::profile_key(',
               'bool EdenServices::choose_profile(', 'int EdenServices::add_profile(',
               'bool EdenServices::rename_profile(', 'bool EdenServices::remove_profile(',
               'void EdenServices::will_play(')
    (out / 'launcher_admission_methods.inc').write_text('\n'.join(
        signature + source.split(signature, 1)[1].split('\n}', 1)[0] + '\n}' for signature in methods))
    (out / 'launcher_admission_members.inc').write_text(
        'std::mutex bridge_;' + header.split('std::mutex bridge_;', 1)[1].split('Eden::CheatManagerRequest CheatRequest', 1)[0])
    sources = [root / 'headless' / name for name in ('cheat_manager.cpp', 'cheat_store.cpp', 'cheat_catalog.cpp',
        'cheat_download.cpp', 'amiibo_sync.cpp', 'amiibo_sync_format.cpp', 'usb_backup.cpp',
        'nextendo.cpp', 'nextendo_http.cpp', 'nextendo_oauth.cpp')]
    inputs = set(sources) | {Path(__file__), source_path, header_path, fixture_path}
    inputs.update(p for p in (root / 'headless').rglob('*') if p.is_file() and p.suffix in ('.h', '.hpp', '.inc'))
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}
    receipt = {'inputs': hashes, 'commands': [], 'result': 'FAILED',
        'fixture_additions': ['Game::network=false', 'network_mutation_active_=false'],
        'production_method_policy': 'verbatim; unchanged guards, USB/core worker code, and actual Nextendo adapter'}
    def run(command, name, **kw):
        result = subprocess.run(command, capture_output=True, text=True, **kw)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        receipt['commands'].append({'argv': command, 'returncode': result.returncode})
        if result.returncode: print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
    try:
        for name, flags in (('normal', []), ('tsan', ['-fsanitize=thread', '-fno-omit-frame-pointer']),
                            ('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])):
            binary = out / name
            run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
                 *flags, '-I', str(out), '-I', str(root / 'headless'), str(out / 'composed_check.cpp'),
                 *map(str, sources), '-lssl', '-lcrypto', '-lz', '-pthread', '-o', str(binary)], name + '-compile')
            run([str(binary)], name + '-run', timeout=90)
            print('Nextendo composed admission ' + name + ': PASS (actual public web/userinfo/NEX, one-shot consent, final-callback busy, no game-token HTTP checks)', flush=True)
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in hashes.items())
        receipt['result'] = 'PASS: normal + TSan + ASan/UBSan; exact production methods, neutral fixture metadata only'
    finally:
        (out / 'receipt.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')


if __name__ == '__main__':
    main()
