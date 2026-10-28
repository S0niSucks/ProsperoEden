#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile exact production grid geometry/D-pad cases; collaborators are inert, not UI evidence."""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/prosperoeden/pe/ui/settings.cpp').read_text()
category = re.search(r'enum Category\s*(?::\s*int\s*)?\{.*?\};', source, re.S)
assert category
geometry = source[source.index('constexpr int kTileColumns'):source.index('// The Video dialog')]
method = source[source.index('void Launcher::press_settings'):source.index('void Launcher::draw_settings')]
navigation = method[method.index('    case Key::up:'):method.index('    case Key::cross:')]
fixture = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <queue>
#include <set>
struct Rect { float x,y,w,h; };
namespace layout { constexpr float kLeft=320.0f, kWidth=1500.0f; }
@CATEGORY@
@GEOMETRY@
enum class Key { up, down, left, right };
struct Harness {
    struct Settings {
        int selected=0;
        void move(int delta) { selected=std::clamp(selected+delta,0,kCategoryCount-1); }
    } settings_;
    struct Theme { bool list_settings=false; } theme;
    bool rail=false;
    const Theme& th() const { return theme; }
    void focus_rail() { rail=true; }
    void press(Key key) {
        int& at=settings_.selected;
        switch (key) {
@NAVIGATION@
        }
        at=std::clamp(at,0,kCategoryCount-1);
    }
};
int main() {
    static_assert(kTilesPerPage==16);
    std::queue<int> pending;
    std::set<int> reached{0}; pending.push(0);
    while(!pending.empty()) {
        const int at=pending.front(); pending.pop();
        for(auto key:{Key::up,Key::down,Key::left,Key::right}) {
            Harness menu; menu.settings_.selected=at; menu.press(key);
            const int next=menu.settings_.selected;
            assert(next>=0 && next<kCategoryCount);
            if(reached.insert(next).second) pending.push(next);
        }
    }
    assert(reached.size()==kCategoryCount);
    // All first-page positions stay exactly where they were before composition.
    for(int at=0;at<kCategoryCount;++at) {
        const auto r=tile_rect(at);
        const int local=at%kTilesPerPage;
        assert(r.x==layout::kLeft+(kTileWidth+kTileGap)*(local%kTileColumns));
        assert(r.y==kTilesTop+(kTileHeight+kTileGap)*(local/kTileColumns));
        assert(r.y+r.h<kStrip.y);
    }
    for(int at=0;at<kCategoryCount;++at) {
        Harness menu; menu.settings_.selected=at; menu.press(Key::right);
        if(at%kTileColumns==kTileColumns-1 || at==kCategoryCount-1)
            assert(menu.settings_.selected==at);
        menu.settings_.selected=at; menu.press(Key::up);
        assert(menu.settings_.selected==(at>=kTileColumns?at-kTileColumns:at));
    }
    if(kCategoryCount>kTilesPerPage) {
        Harness menu; menu.settings_.selected=kTilesPerPage-kTileColumns;
        menu.press(Key::down); assert(menu.settings_.selected==kTilesPerPage);
        menu.press(Key::up); assert(menu.settings_.selected==kTilesPerPage-kTileColumns);
    }
    Harness list; list.theme.list_settings=true;
    for(int i=1;i<kCategoryCount;++i) list.press(Key::down);
    assert(list.settings_.selected==kCategoryCount-1);
}
'''.replace('@CATEGORY@', category[0]).replace('@GEOMETRY@', geometry).replace('@NAVIGATION@', navigation)
with tempfile.TemporaryDirectory(prefix='settings-grid-') as temporary:
    folder = Path(temporary)
    test = folder / 'check.cpp'
    test.write_text(fixture)
    for sanitizer in ([], ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']):
        binary = folder / ('check-sanitized' if sanitizer else 'check')
        subprocess.run([os.environ.get('HOST_CXX', 'clang++-18'), '-std=c++20', '-O1', '-g',
                        '-Wall', '-Wextra', '-Werror', *sanitizer, str(test), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('Settings grid exact geometry/D-pad cases PASS: all categories reachable, bounded page slots, '
      'unchanged first-page placement, terminal/list navigation; normal+ASan/UBSan. '
      'Not full UI/console qualification. Source SHA256=' + hashlib.sha256(source.encode()).hexdigest())
