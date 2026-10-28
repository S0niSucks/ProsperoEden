#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Private fixtures only: additive batch validation must not overwrite existing work."""
import copy
import importlib.util
import json
from pathlib import Path
import tempfile

spec = importlib.util.spec_from_file_location('import_translations', Path(__file__).with_name('import-translations.py'))
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
print('Translation import PASS: validate-before-write, exact locales/source/placeholder binding, repeat no-op, existing bytes preserved and conflicting/blank/unknown input refused')
