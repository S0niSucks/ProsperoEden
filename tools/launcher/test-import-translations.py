#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private fixtures only: additive batch validation must not overwrite existing work."""
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import tempfile

# 251c732 predates the parent's importer. Use its read-only reference without backporting or
# modifying the production importer; after parent composition the neighboring default is used.
importer = Path(os.environ.get('LAUNCHER_TRANSLATION_IMPORTER', Path(__file__).with_name('import-translations.py')))
spec = importlib.util.spec_from_file_location('import_translations', importer)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='prosperoeden-locales-') as temporary:
    root = Path(temporary)
    originals = {}
    for locale in ('pt-BR', 'fr-FR'):
        path = root / (locale + '.po')
        originals[path] = b'# Keep this comment and ordering\nmsgid "Existing"\nmsgstr "Original"\n'
        path.write_bytes(originals[path])
    texts = {'Existing': ['old.cpp'], 'New {0}': ['feature.inc'], 'Quoted': ['feature.inc']}
    data = {'keys': ['New {0}', 'Quoted'],
            'translations': {locale: ['Translation {0}', 'A "quote"\nSecond line'] for locale in ('pt-BR', 'fr-FR')}}
    generated = module.prepare(data, root, texts)
    assert len(generated) == 2
    assert all(path.read_bytes() == originals[path] for path in originals)  # preview does not write
    assert all(content.startswith(originals[path]) for path, content in generated.items())
    assert all(b'feature.inc' in content for content in generated.values())
    for path, content in generated.items():
        path.write_bytes(content)
    assert module.prepare(data, root, texts) == {}  # repeat is a no-op, not duplicate gettext entries
    assert all(module.strings.parse_po(path)['Quoted'] == 'A "quote"\nSecond line' for path in originals)
    invalid = []
    value = copy.deepcopy(data); value['keys'][0] = 'Unknown'; invalid.append(value)
    value = copy.deepcopy(data); value['keys'][1] = value['keys'][0]; invalid.append(value)
    value = copy.deepcopy(data); value['translations'].pop('fr-FR'); invalid.append(value)
    value = copy.deepcopy(data); value['translations']['../../outside'] = []; invalid.append(value)
    value = copy.deepcopy(data); value['translations']['pt-BR'] = []; invalid.append(value)
    for bad in ('', 'Wrong {1}', 'Changed existing {0}', None, 'Control\x00'):
        value = copy.deepcopy(data); value['translations']['pt-BR'][0] = bad; invalid.append(value)
    for value in invalid:
        try:
            module.prepare(value, root, texts)
        except ValueError:
            pass
        else:
            raise AssertionError(value)
        assert all(path.read_bytes() == generated[path] for path in originals)
    try:
        json.loads('{"keys":[],"keys":[]}', object_pairs_hook=module.unique_object)
    except ValueError:
        pass
    else:
        raise AssertionError('Duplicate JSON keys accepted')

spec = importlib.util.spec_from_file_location('feature_strings', Path(__file__).with_name('strings.py'))
strings = importlib.util.module_from_spec(spec)
spec.loader.exec_module(strings)
texts = strings.marked_text()
backends = strings.NETWORK_SOURCES
keys = sorted(key for key, origins in texts.items() if any(origin in backends for origin in origins))
assert len(keys) == 19
with tempfile.TemporaryDirectory(prefix='prosperoeden-network-locales-') as temporary:
    root = Path(temporary)
    originals = {}
    for locale in ('pt-BR', 'fr-FR'):
        path = root / (locale + '.po')
        originals[path] = b'# Keep existing bytes and .inc provenance\n#: feature.inc\nmsgid "On"\nmsgstr "Original"\n'
        path.write_bytes(originals[path])
    data = {'keys': keys, 'translations': {locale: ['Fixture: ' + key for key in keys] for locale in ('pt-BR', 'fr-FR')}}
    generated = module.prepare(data, root, texts)
    assert len(generated) == 2
    for path, content in generated.items():
        assert path.read_bytes() == originals[path] and content.startswith(originals[path])
        assert b'network/config.cpp' in content and b'network/editor.cpp' in content
        path.write_bytes(content)
        entries = module.strings.parse_po(path)
        assert entries['On'] == 'Original' and all(entries[key] == 'Fixture: ' + key for key in keys)
    assert module.prepare(data, root, texts) == {}

if handoff := os.environ.get('NETWORK_TRANSLATION_HANDOFF'):
    raw = Path(handoff).read_bytes()
    assert hashlib.sha256(raw).hexdigest() == '304bc230c8291da0912832a32a07a4f139911f264d8b7e61bf63fa4dcbdc5905'
    data = json.loads(raw, object_pairs_hook=module.unique_object)
    assert len(data['keys']) == 81 and len(data['translations']) == 30
    assert set(keys) <= set(data['keys']) <= texts.keys()
    with tempfile.TemporaryDirectory(prefix='prosperoeden-network-handoff-') as temporary:
        root = Path(temporary)
        originals = {}
        for locale in data['translations']:
            path = root / (locale + '.po')
            originals[path] = b'# Preserve existing catalog bytes\n#: feature.inc\nmsgid "On"\nmsgstr "Existing fixture"\n'
            path.write_bytes(originals[path])
        generated = module.prepare(data, root, texts)
        assert len(generated) == 30 and all(content.startswith(originals[path]) for path, content in generated.items())
        assert all(path.read_bytes() == original for path, original in originals.items())
        # Reject a broken real feature placeholder before writing any fixture.
        bad = copy.deepcopy(data)
        index = next(index for index, key in enumerate(data['keys']) if '{0}' in key)
        bad['translations']['pt-BR'][index] = bad['translations']['pt-BR'][index].replace('{0}', '{1}')
        try:
            module.prepare(bad, root, texts)
        except ValueError:
            pass
        else:
            raise AssertionError('Feature placeholder mutation accepted')
        assert all(path.read_bytes() == original for path, original in originals.items())
        for path, content in generated.items():
            path.write_bytes(content)
        assert module.prepare(data, root, texts) == {}
        assert all(len(module.strings.parse_po(path)) == 82 for path in originals)
    print('SHA-bound handoff PASS: 81 keys x 30 fixture locales, all 19 backend keys, additive/no-op/placeholder refusal')

print('Translation import PASS: validate-before-write, exact locales/source/placeholder binding, repeat no-op, existing bytes/provenance preserved; 19 network keys admitted from real extraction')
