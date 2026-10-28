#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual app theme credit + NX install/recovery. Docker, offline, new private scratch only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from admission_fixture import service_include, scope


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True, help='NEW private output directory')
    args = parser.parse_args()
    assert Path('/.dockerenv').exists(), 'Docker only'
    root = Path(__file__).resolve().parents[1]
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=False)
    ui = root / 'headless/prosperoeden'
    header = (ui / 'eden_services.h').read_text()
    declaration = re.search(r'    std::shared_ptr<void> reserve_theme_import_storage\([^;]+;', header).group()
    member = re.search(r'    std::shared_ptr<Eden::ThemeStorage::Ledger> theme_storage_[^;]+;', header).group()
    (out / 'theme_storage_members.inc').write_text(declaration.replace(' override;', ';') + '\n' + member + '\n')
    # Extract the actual admitted prefix. Parent guards are never unconditionally duplicated.
    records = []
    usb = service_include(ui / 'usb_backup_services.inc', records).split('    if (usb_thread_.joinable())', 1)[0]
    usb = usb[usb.index('bool EdenServices::begin_usb_backup()'):]
    usb = usb.replace('begin_usb_backup()', 'begin_usb_backup_admission()', 1)
    (out / 'theme_storage_usb_handoff.inc').write_text(usb + '\n    return true;\n}\n')
    report = scope(out, records, ('begin_usb_backup',))
    # Use the actual Services default implementation, not a copied default/fake token.
    service = (ui / 'pe/ui/services.hpp').read_text()
    pure = re.findall(r'virtual\s+([^{};]+?)\s*=\s*0\s*;', service)
    assert len(pure) == 26, 'Services abstract surface changed: update the focused fixture'
    methods = []
    for method in pure:
        body = '{ return {}; }'
        if re.match(r'const\s+std::vector<std::string>\s*&', method):
            body = '{ static const std::vector<std::string> value; return value; }'
        methods.append(method + ' override ' + body)
    (out / 'theme_storage_default.inc').write_text('namespace pe::ui {\nstruct DefaultThemeServices final : Services {\n' +
        '\n'.join(methods) + '\n};\n}\nusing pe::ui::DefaultThemeServices;\n')
    # Exact ninth-argument expression with the real service/runtime headers. Not a full frontend
    # or target build: platform/GL setup and SDK imports are deliberately not qualified here.
    frontend = (ui / 'frontend.cpp').read_text()
    call = re.search(r'        pe::ui::RuntimeLauncher launcher\([\s\S]+?\n            \}\);', frontend).group()
    assert 'services.reserve_theme_import_storage(root, bytes)' in call
    (out / 'frontend_caller.cpp').write_text('''#include "eden_services.h"
#include "pe/ui/runtime_launcher.hpp"
#include "storage_paths.h"
void CheckCaller(EdenServices& services, pe::ui::Textures& textures, const pe::ui::Fonts& fonts,
    pe::gfx::GlBatch& batch, bool first_start, bool audio_ready, std::unique_ptr<pe::audio::Mixer>& mixer) {
    auto LoadLanguage = [](pe::gfx::Font&) {};
''' + call + '\n}\n')
    (out / 'native_unknown.cpp').write_text('''#include "theme_storage.h"
#include <cassert>
int main() { std::uint64_t bytes = 7; assert(!Eden::ThemeStorage::AvailableBytes(-1, bytes) && bytes == 0); }
''')
    inputs = {Path(__file__), Path(__file__).with_name('admission_fixture.py'), root / 'headless/theme_storage_check.cpp', ui / 'frontend.cpp',
              ui / 'usb_backup_services.inc', ui / 'pe/audio/wav.cpp'}
    inputs.update(p for p in (root / 'headless').rglob('*') if p.is_file() and p.suffix in ('.h', '.hpp', '.inc'))
    inputs.update(p for p in (root / 'third_party').rglob('*') if p.is_file() and p.suffix in ('.h', '.hpp', '.inl'))
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(inputs)}
    receipt = {'inputs': hashes, 'commands': [], 'result': 'FAILED',
        'fixture_scope': report,
        'scope': 'actual theme service method + installer/recovery; synthetic statuses/capacity; actual USB prefix, optionally labeled overlay; host caller expression syntax, not native app'}
    flags = [os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g1', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
        '-Wno-unused-parameter', '-I' + str(out), '-I' + str(root / 'headless'), '-I' + str(ui),
        '-I' + str(root / 'third_party'), '-I' + str(root / 'third_party/miniz')]
    def run(command, name, **kw):
        result = subprocess.run(command, capture_output=True, text=True, **kw)
        (out / (name + '.log')).write_text(result.stdout + result.stderr)
        receipt['commands'].append({'argv': command, 'returncode': result.returncode})
        if result.returncode: print(result.stdout + result.stderr, flush=True)
        result.check_returncode()
        return result.stdout
    try:
        run([*flags, '-fsyntax-only', str(out / 'frontend_caller.cpp')], 'frontend-caller-syntax')
        print('Frontend exact ninth-argument expression, real service/runtime headers: host syntax PASS', flush=True)
        run([*flags, '-D__PROSPERO__', str(out / 'native_unknown.cpp'), '-o', str(out / 'native-unknown')], 'native-unknown-compile')
        run([str(out / 'native-unknown')], 'native-unknown-run')
        for mode, sanitizers in (('normal', []), ('asan-ubsan', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']),
                                 ('tsan', ['-fsanitize=thread', '-fno-omit-frame-pointer'])):
            binary = out / mode
            # Keep existing compact scene-header indentation warnings visible without editing
            # library-owned code. All other warnings/errors and sanitizer assertions stay strict.
            run([*flags, '-Wno-error=misleading-indentation', *sanitizers, str(root / 'headless/theme_storage_check.cpp'), str(ui / 'pe/audio/wav.cpp'),
                 '-Wl,--wrap=fstatvfs,--wrap=write,--wrap=fsync,--wrap=linkat,--wrap=unlinkat',
                 '-lcrypto', '-pthread', '-o', str(binary)], mode + '-compile')
            print(run([str(binary)], mode + '-run', timeout=90).strip() + ' [' + mode + ']', flush=True)
            Path('/data/prosperoeden').rename('/data/prosperoeden-' + mode)
        assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in hashes.items())
        receipt['result'] = 'PASS normal + ASan/UBSan + TSan; exact host caller syntax; native unknown capacity denies'
    finally:
        (out / 'receipt.json').write_text(json.dumps(receipt, sort_keys=True, indent=2) + '\n')


if __name__ == '__main__':
    main()
