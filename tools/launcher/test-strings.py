#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-discovery regression: feature UI includes must not silently miss localization."""
import importlib.util
from pathlib import Path
import tempfile

spec = importlib.util.spec_from_file_location('launcher_strings', Path(__file__).with_name('strings.py'))
strings = importlib.util.module_from_spec(spec)
spec.loader.exec_module(strings)

with tempfile.TemporaryDirectory(prefix='prosperoeden-strings-') as temporary:
    root = Path(temporary)
    strings.LAUNCHER = root
    strings.SETTINGS = root / 'settings_store.h'
    strings.SETTINGS.write_text('\n'.join(name + '[] = {"Setting"};' for name in strings.SETTING_LABELS), encoding='utf-8')
    for extension in ('cpp', 'hpp', 'h', 'inc'):
        (root / ('feature.' + extension)).write_text(
            'const auto label = tr("' + extension + ' label");\n'
            'const auto table = TR("Shared label");\n'
            'const auto paragraph = tr("First " "second");\n'
            '// tr("Comment-only text")\n', encoding='utf-8')
    (root / 'not-source.txt').write_text('tr("Ignored file")', encoding='utf-8')
    texts = strings.marked_text()
    assert set(texts) == {'cpp label', 'hpp label', 'h label', 'inc label',
                          'Shared label', 'First second', 'Setting'}, texts
    assert texts['inc label'] == ['feature.inc']
    assert len(texts['Shared label']) == 4
    assert texts['Setting'] == ['settings_store.h'] * len(strings.SETTING_LABELS)

print('Launcher strings PASS: C++/header/include discovery, shared msgids, adjacent literals and comment/non-source exclusion')
