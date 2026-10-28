#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source-discovery regression: preserve UI includes and whitelist only marked network returns."""
import importlib.util
import json
import os
import re
from pathlib import Path
import tempfile

def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

strings = load(Path(__file__).with_name('strings.py'), 'launcher_strings')
original = strings.ROOT, strings.LAUNCHER, strings.SETTINGS, strings.NETWORK_SOURCES

with tempfile.TemporaryDirectory(prefix='prosperoeden-strings-') as temporary:
    root = Path(temporary)
    strings.ROOT = root
    strings.LAUNCHER = root / 'headless/prosperoeden'
    strings.LAUNCHER.mkdir(parents=True)
    strings.SETTINGS = root / 'headless/settings_store.h'
    strings.SETTINGS.write_text('\n'.join(name + '[] = {"Setting"};' for name in strings.SETTING_LABELS), encoding='utf-8')
    for extension in ('cpp', 'hpp', 'h', 'inc'):
        (strings.LAUNCHER / ('feature.' + extension)).write_text(
            'const auto label = tr("' + extension + ' label");\n'
            'const auto table = TR("Shared label");\n'
            'const auto paragraph = tr("First " "second");\n'
            '// tr("Comment-only text")\n', encoding='utf-8')
    (strings.LAUNCHER / 'not-source.txt').write_text('tr("Ignored file")', encoding='utf-8')
    texts = strings.marked_text()
    assert set(texts) == {'cpp label', 'hpp label', 'h label', 'inc label',
                          'Shared label', 'First second', 'Setting'}, texts
    assert texts['inc label'] == ['feature.inc']
    assert len(texts['Shared label']) == 4
    assert texts['Setting'] == ['settings_store.h'] * len(strings.SETTING_LABELS)
    network = root / 'headless/network'; network.mkdir()
    for name in ('config', 'editor'):
        (network / (name + '.cpp')).write_text(
            'return TR("Backend ' + name + '");\n'
            'return TR("Shared label");\n'
            'return TR("Adjacent " "backend");\n'
            'return "Unmarked private diagnostic";\n'
            '// TR("Backend comment")\n', encoding='utf-8')
    (network / 'keyboard.cpp').write_text('return TR("Prompt title");\nreturn "Unmarked prompt";', encoding='utf-8')
    (network / 'transport.cpp').write_text('return TR("Not an allowed backend");', encoding='utf-8')
    (network / 'keyboard_prompt.h').write_text('return TR("Not an allowed backend header");', encoding='utf-8')
    (network / 'config.inc').write_text('return TR("Not an allowed extension");', encoding='utf-8')
    (root / 'headless/main.cpp').write_text('return TR("Not a UI source");', encoding='utf-8')
    marked = strings.marked_text()
    assert set(marked) - set(texts) == {'Backend config', 'Backend editor', 'Adjacent backend', 'Prompt title'}
    assert marked['Prompt title'] == ['network/keyboard.cpp']
    assert marked['Backend config'] == ['network/config.cpp']
    assert marked['Backend editor'] == ['network/editor.cpp']
    assert marked['Adjacent backend'] == ['network/config.cpp', 'network/editor.cpp']
    for key, origins in texts.items():
        assert marked[key][:len(origins)] == origins
    assert marked['Shared label'][4:] == ['network/config.cpp', 'network/editor.cpp']

strings.ROOT, strings.LAUNCHER, strings.SETTINGS, strings.NETWORK_SOURCES = original
strings.NETWORK_SOURCES = ()
before = strings.marked_text()
strings.NETWORK_SOURCES = strings.NETWORK_BACKEND_SOURCES
backend = strings.marked_text()
strings.NETWORK_SOURCES = original[3]
after = strings.marked_text()
handoff = {json.loads(line) for line in (strings.ROOT / 'docs/NETWORK_EDITOR_MSGIDS.txt').read_text(encoding='utf-8').splitlines()
           if line and not line.startswith('#')}
assert len(handoff) == 83
expected = handoff - before.keys()  # use the existing handoff, not a duplicate backend message table
backend_added = backend.keys() - before.keys()
original_added = backend_added - {'Network location'}
assert len(expected) == 19 and original_added == expected, (expected, original_added)
assert len(backend_added) == 20 and backend_added == expected | {'Network location'}
# Independent frozen audit from 1ec3910: exact 14 titles + two hints, not a count inferred
# merely from growing the allowlist. The runtime literals stay single-source in keyboard.cpp.
audit = (strings.ROOT / 'docs/NETWORK_TEXT_EXTRACTION.md').read_text(encoding='utf-8')
prompt_expected = set(re.findall(r'^\| line \d+, (?:title|password hint|other hint) \| `([^`]+)` \|$', audit, re.MULTILINE))
prompt_added = after.keys() - backend.keys()
assert len(prompt_expected) == 16 and prompt_added == prompt_expected, (prompt_expected, prompt_added)
added = after.keys() - before.keys()
assert len(added) == 36
for key, origins in before.items():
    assert after[key] == origins, (key, origins, after[key])
assert all(set(after[key]) <= set(strings.NETWORK_BACKEND_SOURCES) for key in backend_added)
assert all(after[key] == ['network/keyboard.cpp'] for key in prompt_added)
assert after['Network location'] == ['network/editor.cpp']

# The generated template and every shipped catalog must cover the same current source set.
# Check named tokens as well as numeric formatting arguments: oauth redirect hints use {port}.
assert strings.parse_po(strings.TEMPLATE).keys() == after.keys(), 'Regenerate launcher.pot from current sources'
catalogs = sorted(strings.CATALOGS.glob('*.po'))
assert catalogs
for catalog in catalogs:
    translations = strings.parse_po(catalog)
    assert translations.keys() == after.keys(), catalog.name
    for key, value in translations.items():
        assert value.strip(), (catalog.name, key)
        assert sorted(re.findall(r'\{[\w]+\}', key)) == sorted(re.findall(r'\{[\w]+\}', value)), (catalog.name, key)
    for key in prompt_expected:
        value = translations[key]
        assert '\0' not in value and len(value.encode('utf-16-le')) // 2 <= 127, (catalog.name, key)
print('Catalog contract PASS: current template/locale coverage, named/numeric tokens and native prompt UTF-16 limits')

# Optional external coverage receipt is read-only and independent of our marker implementation.
if path := os.environ.get('NETWORK_TRANSLATION_COVERAGE'):
    coverage = json.loads(Path(path).read_text(encoding='utf-8'))
    assert original_added == set(coverage['feature_needs_extraction_marking'])

# Compose only discovery in memory over the current parent sources; never edit the parent checkout.
if path := os.environ.get('NETWORK_STRINGS_REFERENCE_ROOT'):
    reference = load(Path(path) / 'tools/launcher/strings.py', 'parent_strings')
    baseline = reference.marked_text()
    strings.LAUNCHER, strings.SETTINGS = reference.LAUNCHER, reference.SETTINGS
    composed = strings.marked_text()  # ROOT still selects this branch's explicit network sources
    assert composed.keys() - baseline.keys() == added - baseline.keys()
    assert all(composed[key][:len(origins)] == origins for key, origins in baseline.items())
    print(f'Parent source/provenance preservation PASS: {len(baseline)} existing msgids, .inc retained; 20 backend + 16 prompt keys scoped separately')

print('Launcher strings PASS: legacy discovery/provenance; original 19 exact handoff keys separately preserved; 20 backend keys + 16 exact native prompt keys (not full UI composition)')
