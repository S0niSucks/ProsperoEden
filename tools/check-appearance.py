#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual settings/palette/navigation C++ and provider-body round trips.

Run in the Docker host-check image. The provider bodies are extracted unchanged
except for redirecting their default settings path to a disposable fixture.
This is not selected-core/native SDK compilation or console rendering evidence.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/prosperoeden/eden_services.cpp').read_text(encoding='utf-8')
if sys.argv[1:] not in ([], ['--preferences-only']):
    raise SystemExit('Usage: check-appearance.py [--preferences-only]')
if '--preferences-only' not in sys.argv:
    switch = (root / 'headless/prosperoeden/pe/ui/switch_theme.cpp').read_text(encoding='utf-8')
    assert 'selected_ink=theme::palette_id || look().high_contrast ? kBlack : kWhite' in switch
    assert 'i==selected?selected_ink:kInk' in switch
get_start = source.index('pe::ui::Preferences EdenServices::preferences() {')
set_start = source.index('bool EdenServices::set_preferences(', get_start)
set_end = source.index('\nconst std::vector<std::string>& EdenServices::resolution_labels()', set_start)
bodies = source[get_start:set_end]
assert bodies.count('Eden::LoadPreferences()') == 1
assert bodies.count('Eden::SavePreferences(value)') == 1
bodies = bodies.replace('Eden::LoadPreferences()', 'Eden::LoadPreferences(fixture_file)')
bodies = bodies.replace('Eden::SavePreferences(value)', 'Eden::SavePreferences(value, fixture_file)')
code = r'''
#include "settings_store.h"
#include "pe/ui/services.hpp"
#include "pe/ui/theme.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
std::string fixture_file;
namespace Eden { inline void Report(const char*, const char*) {} }
class EdenServices {
public:
    pe::ui::Preferences preferences();
    bool set_preferences(const pe::ui::Preferences&);
};
PROVIDER_BODIES
int main(int argc, char** argv) {
    assert(argc == 2);
    fixture_file = std::string(argv[1]) + "/settings.json";
    using namespace Eden;
    Settings::Json original = {{"private_future", {{"keep", 37}}},
        {"appearance", {{"ingame_button_glyphs", "switch"}}},
        {"library", {{"last_game", "Title/main"}}}};
    assert(Settings::Write(original, fixture_file));
    EdenServices provider;
    for (int interface = 0; interface < int(Appearance::kInterfaces.size()); ++interface)
        for (int palette = 0; palette < int(Appearance::kPalettes.size()); ++palette)
            for (int layout = 0; layout < 2; ++layout) {
                auto p = provider.preferences();
                p.ui_theme = interface; p.ui_palette = palette; p.library_layout = layout;
                p.fsr_sharpness = 83; p.anti_aliasing = 2;
                assert(provider.set_preferences(p));
                const auto read = provider.preferences();
                assert(read.ui_theme == interface && read.ui_palette == palette && read.library_layout == layout);
                assert(read.fsr_sharpness == 83 && read.anti_aliasing == 2);
                const auto doc = Settings::Load(fixture_file);
                assert(doc.at("private_future").at("keep") == 37);
                assert(doc.at("library").at("last_game") == "Title/main");
                assert(doc.at("appearance").at("ingame_button_glyphs") == "switch");
                assert(doc.at("appearance").at("interface") == Appearance::kInterfaces[interface]);
                assert(doc.at("appearance").at("theme") == (interface == 1 ? "switch" : "encore"));
            }
    auto p = provider.preferences();
    std::string before; assert(Settings::ReadFile(fixture_file, before));
    p.ui_palette = 5; assert(!provider.set_preferences(p));
    p.ui_palette = -1; assert(!provider.set_preferences(p));
    p.ui_palette = 0; p.ui_theme = 3; assert(!provider.set_preferences(p));
    p.ui_theme = -1; assert(!provider.set_preferences(p));
    p.ui_theme = 0; p.library_layout = 2; assert(!provider.set_preferences(p));
    std::string after; assert(Settings::ReadFile(fixture_file, after)); assert(before == after);
    assert(Settings::Write({{"appearance",{{"theme","switch"}}}},fixture_file));
    assert(provider.preferences().ui_theme == 1 && provider.preferences().ui_palette == 0);
    assert(Settings::Write({{"appearance",{{"interface","obsolete"},{"theme","switch"}}}},fixture_file));
    assert(provider.preferences().ui_theme == 0); // invalid canonical value must not revive stale alias
    assert(Settings::Write({{"appearance",{{"theme","unknown"},{"palette",55},{"library_layout","obsolete"}}}},fixture_file));
    p = provider.preferences();
    assert(p.ui_theme == 0 && p.ui_palette == 0 && p.library_layout == 0);
    assert(Settings::Write({{"appearance",37},{"library",{{"last_game","keep.nsp"}}}},fixture_file));
    assert(!provider.set_preferences(p));
    assert(Settings::Load(fixture_file).at("appearance") == 37);
    assert(Settings::Load(fixture_file).at("library").at("last_game") == "keep.nsp");
    assert(Settings::Write({{"appearance",Settings::Json::array()}},fixture_file));
    assert(!provider.set_preferences(p));
    assert(Settings::Load(fixture_file).at("appearance").is_array());
    assert(Settings::Write({{"appearance",nullptr}},fixture_file));
    assert(!provider.set_preferences(p));
    assert(Settings::Load(fixture_file).at("appearance").is_null());
    for (const std::string corrupt : {"17", "[]", "null", "{\"appearance\":"}) {
        assert(Settings::WriteFile(fixture_file,corrupt));
        assert(provider.preferences().ui_theme == 0); // reads may default, mutations must refuse
        assert(!provider.set_preferences(p));
        std::string unchanged; assert(Settings::ReadFile(fixture_file,unchanged) && unchanged==corrupt);
    }
    for (int count = 0; count < 50; ++count)
        for (int columns : {1,4})
            for (int index = 0; index < std::max(1,count); ++index)
                for (const auto [dx,dy] : {std::pair{-1,0}, {1,0}, {0,-1}, {0,1}}) {
                    const int next = Appearance::Move(index,count,columns,dx,dy);
                    assert(next >= 0 && next < std::max(1,count));
                    if (dx && next != index) assert(next / columns == index / columns);
                    if (dy && next != index) assert(next % columns == index % columns);
                }
    assert(Appearance::Move(8,10,4,0,1) == 8);
    assert(Appearance::Move(7,10,4,0,1) == 7);
    assert(Appearance::Move(8,10,4,0,-1) == 4);
    const auto legacy = pe::ui::theme::kBase;
    pe::ui::theme::ApplyPalette(2);
    assert(pe::ui::theme::palette_id == 2 && pe::ui::theme::kBase.g > legacy.g);
    pe::ui::theme::ApplyPalette(999);
    assert(pe::ui::theme::palette_id == 0 && pe::ui::theme::kBase.r == legacy.r);
    const auto luminance=[](pe::gfx::Color color) {
        const auto linear=[](float c) { return c<=0.04045f ? c/12.92f : std::pow((c+0.055f)/1.055f,2.4f); };
        return 0.2126f*linear(color.r)+0.7152f*linear(color.g)+0.0722f*linear(color.b);
    };
    for(int palette=0;palette<5;++palette) {
        pe::ui::theme::ApplyPalette(palette);
        const float ratio=(luminance(pe::ui::theme::kFocusCore)+0.05f)/
                          (luminance(pe::gfx::Color::rgb(0x161616))+0.05f);
        assert(ratio>=4.5f); // raw role-pair check only; real emitted glyphs are asserted in the combined preview
    }
    std::puts("APPEARANCE_SETTINGS_PROVIDER_NAVIGATION_PASS");
}
'''.replace('PROVIDER_BODIES', bodies)
with tempfile.TemporaryDirectory(prefix='encore-appearance-') as tmp:
    exe = Path(tmp) / 'appearance'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra',
                    '-Werror', '-Wno-range-loop-construct', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root / 'headless'),
                    '-I', str(root / 'headless/prosperoeden'),
                    '-x', 'c++', '-', '-o', str(exe)], input=code, text=True, check=True)
    subprocess.run([str(exe), tmp], check=True, timeout=30)
    if '--preferences-only' in sys.argv:
        print('APPEARANCE_PREFERENCES_ONLY_PASS (UI source seams excluded)')
