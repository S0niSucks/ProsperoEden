#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Test-only verbatim extraction; optional, explicitly reported private predicate overlay."""
import hashlib
import json
import os
from pathlib import Path
import re

THEME_RETURNS = {
    'can_launch_game': 'false', 'set_files_folder': 'false', 'set_games_folder': 'false',
    'choose_profile': 'false', 'add_profile': '-1', 'rename_profile': 'false', 'remove_profile': 'false',
    'will_play': '', 'begin_usb_backup': 'false', 'start_update': 'false', 'apply_update': 'false',
    'check_cheat_database': 'false', 'install_cheat_database': 'false', 'start_amiibo_sync': 'false',
    'download': 'false', 'start_pairing': 'false', 'refresh_sources': '', 'start_save_sync': '',
    'reserve_network_job': '{}',
    'nextendo_action_with_consent': '{false, false, false, 0, "Nextendo action failed. Retry or unlink the session."}',
}


def definition(text, name):
    match = re.search(r'^.*\b' + re.escape(name) + r'\(', text, re.M)
    assert match, name
    start = text.index('{', match.start()); depth = 0
    for token in re.finditer(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', text[start:], re.S):
        if token[0] == '{': depth += 1
        elif token[0] == '}':
            depth -= 1
            if not depth: return text[match.start():start + token.end()]
    raise AssertionError(name)


def extract(text, name, records):
    original = body = definition(text, name)
    short = name.removeprefix('EdenServices::')
    if os.environ.get('ADMISSION_THEME_OVERLAY') == '1' and short in THEME_RETURNS:
        predicates = []
        if 'theme_storage_->busy()' not in body: predicates.append('theme_storage_->busy()')
        if short == 'nextendo_action_with_consent' and 'network_mutation_active_' not in body:
            predicates.append('network_mutation_active_')
        if predicates:
            # Only the generated fixture body changes. No source checkout/production patching.
            lock = re.search(r'^(\s*)const std::(?:lock_guard|unique_lock) admission\(job_admission_mutex_[^;]*;\n', body, re.M)
            assert lock, name
            value = THEME_RETURNS[short]
            line = lock[1] + 'if (' + ' || '.join(predicates) + ') return' + (' ' + value if value else '') + ';\n'
            body = body[:lock.end()] + line + body[lock.end():]
    records.append({'name': name, 'verbatim': body == original,
        'source_sha256': hashlib.sha256(original.encode()).hexdigest(),
        'fixture_sha256': hashlib.sha256(body.encode()).hexdigest(),
        'theme_guard': 'theme_storage_->busy()' in body,
        'network_guard': 'network_mutation_active_' in body})
    return body


def service_include(path, records):
    text = Path(path).read_text()
    for short in THEME_RETURNS:
        name = 'EdenServices::' + short
        if name + '(' in text:
            before = definition(text, name)
            text = text.replace(before, extract(text, name, records), 1)
    return text


def scope(folder, records, required):
    by_name = {r['name'].removeprefix('EdenServices::'): r for r in records}
    guarded = [name for name in required if by_name[name]['theme_guard']]
    enabled = bool(guarded) or os.environ.get('ADMISSION_REQUIRE_THEME') == '1'
    if enabled:
        assert len(guarded) == len(required), 'Incomplete theme contract: ' + str(set(required) - set(guarded))
    nextendo = by_name.get('nextendo_action_with_consent')
    network = bool(nextendo and nextendo['network_guard'])
    report = {'policy': 'PRIVATE PREDICATE OVERLAY' if any(not r['verbatim'] for r in records) else 'VERBATIM PRODUCTION',
              'theme_contract_exercised': enabled, 'nextendo_network_guard': network, 'methods': records}
    folder = Path(folder)
    (folder / 'admission_fixture_scope.json').write_text(json.dumps(report, sort_keys=True, indent=2) + '\n')
    (folder / 'admission_fixture_contract.inc').write_text(
        f'#define ADMISSION_THEME_CONTRACT {int(enabled)}\n#define ADMISSION_NEXTENDO_NETWORK_CONTRACT {int(network)}\n')
    print('Fixture scope: ' + report['policy'] + '; theme contract ' + ('ENABLED' if enabled else 'NOT PRESENT in source; not claimed'), flush=True)
    return report
