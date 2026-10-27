#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the actual folder-browser actions with independent game/data locations."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browse = (root / 'headless/prosperoeden/pe/ui/browse.cpp').read_text()


def function(signature):
    start = browse.index(signature)
    return browse[start:browse.index('\n}\n', start) + 3]


settings = (root / 'headless/prosperoeden/pe/ui/settings.cpp').read_text()
assert 'enter_files(true);' in settings and 'TR("ROMs and updates")' in settings
backend = (root / 'headless/prosperoeden/eden_services.cpp').read_text()
assert 'Eden::SaveGamesDir(directory)' in backend
assert 'EdenServices::saved_games_folder() { return Eden::LoadSavedGamesDir(); }' in backend
assert 'browse_games_ ? services_.games_folder() : services_.files_folder()' in browse

code = r'''
#include <cassert>
#include <string>
#include <vector>
enum class Key { circle, up, down, l1, r1, cross, triangle, square, options };
enum class Screen { settings };
enum class Cue { focus, page, error, select, saved };
const char* tr(const char* text) { return text; }
struct List {
    int visible{}, selected{}, count{};
    float pitch{};
    bool move(int) { return false; }
    bool page(int) { return false; }
    void reset(int n, int at) { count = n; selected = at; }
};
struct Services {
    std::string data = "/data/prosperoeden", pending_data, pending_games;
    bool fail = false, missing = false;
    std::string saved_files_folder() { return pending_data; }
    std::string files_folder() { return data; }
    std::string saved_games_folder() { return pending_games; }
    std::string games_folder() { return "/mnt/usb0/games"; }
    std::string default_files_folder() { return "/data/prosperoeden"; }
    bool set_files_folder(const std::string& directory) {
        if (fail) return false;
        pending_data = directory; return true;
    }
    bool set_games_folder(const std::string& directory) {
        if (fail) return false;
        pending_games = directory; return true;
    }
};
struct Launcher {
    Services services_;
    List files_;
    bool browse_games_{}, message_warning_{}, backed_out{};
    std::string browse_dir_, message_;
    std::vector<std::string> browse_entries_;
    void enter_files(bool games_only = false);
    void press_files(Key key);
    bool browse_to(const std::string& path) {
        if (services_.missing && path != "/") return false;
        browse_dir_ = path; return true;
    }
    void say(const char* text, bool warning) { message_ = text; message_warning_ = warning; }
    void cue(Cue) {}
    void open(Screen, bool) { backed_out = true; }
};
''' + function('std::string join_path(') + function('std::string parent_path(') + \
    function('void Launcher::enter_files(') + function('void Launcher::press_files(') + r'''
int main() {
    Launcher l;
    l.enter_files();
    assert(!l.browse_games_ && l.browse_dir_ == "/data/prosperoeden");
    l.enter_files(true);
    assert(l.browse_games_ && l.browse_dir_ == "/mnt/usb0/games");
    l.browse_dir_ = "/mnt/usb1/library";
    l.press_files(Key::triangle);
    assert(l.services_.pending_games == "/mnt/usb1/library");
    assert(l.services_.pending_data.empty() && !l.message_warning_);
    l.services_.pending_data = "/mnt/ext1/keys";
    l.press_files(Key::square);
    assert(l.services_.pending_games.empty());
    assert(l.services_.pending_data == "/mnt/ext1/keys" && l.browse_dir_ == "/mnt/ext1/keys");
    l.services_.pending_games = "/mnt/usb0/games";
    l.services_.fail = true;
    l.browse_dir_ = "/mnt/usb1/library";
    l.press_files(Key::triangle);
    assert(l.services_.pending_games == "/mnt/usb0/games" && l.message_warning_);
    l.press_files(Key::square);
    assert(l.services_.pending_games == "/mnt/usb0/games" && l.browse_dir_ == "/mnt/usb1/library");
    l.press_files(Key::circle);
    assert(l.backed_out && l.services_.pending_games == "/mnt/usb0/games");
    l.services_.fail = false;
    l.enter_files();
    assert(!l.browse_games_ && l.browse_dir_ == "/mnt/ext1/keys");
    l.browse_dir_ = "/data/new-keys";
    l.press_files(Key::triangle);
    assert(l.services_.pending_data == "/data/new-keys");
    assert(l.services_.pending_games == "/mnt/usb0/games");
    l.press_files(Key::square);
    assert(l.services_.pending_data == "/data/prosperoeden");
    assert(l.services_.pending_games == "/mnt/usb0/games");
    l.services_.missing = true;
    l.enter_files(true);
    assert(l.browse_dir_ == "/" && l.services_.pending_games == "/mnt/usb0/games");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'games-folder'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-x', 'c++', '-', '-o', str(exe)], input=code, text=True, check=True)
    subprocess.run([str(exe)], check=True)
print('Games folder: PASS independent browser save/reset, pending data folder, cancellation, '
      'failed writes and unavailable USB without changing the saved location')
