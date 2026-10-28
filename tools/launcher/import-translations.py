#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Add reviewed feature translations without changing existing catalog bytes.

Usage: import-translations.py DATA.json [--write]
DATA contains keys: [English msgids], translations: {existing locale: [translations]},
and an optional comment. All existing locales and every key must be supplied.
The default validates/previews; --write publishes generated gettext entries.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import re

spec = importlib.util.spec_from_file_location('launcher_strings', Path(__file__).with_name('strings.py'))
strings = importlib.util.module_from_spec(spec)
spec.loader.exec_module(strings)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate JSON field: ' + key)
        result[key] = value
    return result


def prepare(data, catalogs, texts):
    """Validate the entire batch before returning any generated file content."""
    if not isinstance(data, dict) or not {'keys', 'translations'} <= data.keys() or data.keys() - {'keys', 'translations', 'comment'}:
        raise ValueError('Expected keys/translations and optional comment')
    keys, translations = data['keys'], data['translations']
    if not isinstance(keys, list) or not keys or len(keys) > 512 or any(not isinstance(k, str) or not k or len(k) > 8192 for k in keys):
        raise ValueError('Expected 1–512 nonblank bounded msgids')
    if len(set(keys)) != len(keys) or not set(keys) <= texts.keys():
        raise ValueError('Duplicate or unknown source msgid')
    paths = {path.stem: path for path in catalogs.glob('*.po')}
    if not paths or not isinstance(translations, dict) or translations.keys() != paths.keys():
        raise ValueError('Translations must name exactly the existing catalog locales')
    generated = {}
    for locale, path in sorted(paths.items()):
        values = translations[locale]
        if path.is_symlink() or not path.is_file():
            raise ValueError('Catalog must be a regular file: ' + locale)
        if not isinstance(values, list) or len(values) != len(keys):
            raise ValueError('Wrong translation count: ' + locale)
        existing = strings.parse_po(path)
        additions = []
        for key, value in zip(keys, values):
            if not isinstance(value, str) or not value.strip() or len(value) > 16384 or any(ord(c) < 32 and c != '\n' for c in value):
                raise ValueError('Blank, malformed or oversized translation: ' + locale)
            if sorted(re.findall(r'\{\d+\}', key)) != sorted(re.findall(r'\{\d+\}', value)):
                raise ValueError('Placeholder mismatch: ' + locale + ': ' + key)
            if key in existing:
                if existing[key] != value:
                    raise ValueError('Refusing to replace existing translation: ' + locale + ': ' + key)
                continue
            additions.extend(['#: ' + ', '.join(sorted(set(texts[key]))),
                              'msgid "' + strings.escape(key) + '"',
                              'msgstr "' + strings.escape(value) + '"', ''])
        if additions:
            original = path.read_bytes()
            generated[path] = original + b'\n\n' + '\n'.join(additions).encode('utf-8')
    return generated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('data', type=Path)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    raw = args.data.read_bytes()
    if len(raw) > 2 * 1024 * 1024:
        raise ValueError('Translation batch exceeds 2 MiB')
    data = json.loads(raw, object_pairs_hook=unique_object)
    generated = prepare(data, strings.CATALOGS, strings.marked_text())
    for path, content in generated.items():
        if args.write:
            path.write_bytes(content)
    print(f'{len(data["keys"])} msgids, {len(data["translations"])} locales, '
          f'{len(generated)} catalogs ' + ('generated' if args.write else 'validated (no writes)'))


if __name__ == '__main__':
    main()
