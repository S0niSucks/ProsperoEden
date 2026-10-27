#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Real bounded PNG decoder: synthetic fixtures, malformed streams and atomic presentation."""
from pathlib import Path
import os
import random
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
stb = Path(sys.argv[1]) / 'externals/stb'
assert (stb / 'stb_image.h').is_file()


def png(width, height, rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


code = r'''
#include "quick_picture.cpp"
#include "quick_overlay.h"
#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <thread>
int main(int argc, char** argv) {
    assert(argc == 2);
    namespace fs = std::filesystem;
    const fs::path root{argv[1]};
    const auto load = [&](const char* name) {
        auto picture = Eden::LoadQuickPicture((root / name).string());
        assert(allocated == 0); // success and every failure release the actual stb allocations
        return picture;
    };
    const auto picture = load("portrait.png"); assert(picture);
    const auto* top = picture->rgba.data() + (10 * 256 + 128) * 4;
    const auto* bottom = picture->rgba.data() + (245 * 256 + 128) * 4;
    assert(top[0] == 255 && top[2] == 0 && top[3] == 255);
    assert(bottom[0] == 0 && bottom[2] == 255 && bottom[3] == 255);
    assert(picture->rgba[(128 * 256 + 1) * 4 + 3] == 0); // aspect-fit transparent side padding
    assert(picture->rgba[(128 * 256 + 255) * 4 + 3] == 0);
    const auto red = load("red.png"), blue = load("blue.png"); assert(red && blue);
    allocated = kDecodeBudget; // actual decoder allocation refusal, not just a failed-file fixture
    assert(!Eden::LoadQuickPicture((root / "red.png").string()));
    assert(allocated == kDecodeBudget); allocated = 0;
    const auto alpha = load("alpha.png"); assert(alpha);
    for (size_t n = 0; n < alpha->rgba.size(); n += 4)
        if (alpha->rgba[n + 3]) assert(alpha->rgba[n] == 255 && alpha->rgba[n + 2] == 0);
    for (const auto* name : {"missing.png", "fifo.png", "alias.png", "alias/red.png", "bad.png",
                            "wide.png", "zero.png", "bomb.png", "huge.png", "short.png"}) assert(!load(name));
    for (const auto& file : fs::directory_iterator(root / "fuzz")) {
        (void)Eden::LoadQuickPicture(file.path().string()); assert(allocated == 0);
    }
    std::atomic<bool> start{}, stop{};
    std::thread reader([&] {
        start = true;
        while (!stop) {
            const auto frame = Eden::QuickOverlay::Read();
            if (!frame.text[0]) { assert(!frame.image); continue; }
            assert(frame.image == (frame.text[0] == 'R' ? red : blue));
        }
    });
    while (!start) std::this_thread::yield();
    for (unsigned n = 0; n < 10000; ++n) {
        Eden::QuickOverlay::Show("RED", red); Eden::QuickOverlay::Show("BLUE", blue);
        if (n % 7 == 0) Eden::QuickOverlay::Hide();
    }
    stop = true; reader.join(); Eden::QuickOverlay::Hide();
    std::weak_ptr<const Eden::QuickPicture> lifetime;
    {
        auto image = load("red.png"); lifetime = image;
        Eden::QuickOverlay::Show("RED", image); image.reset();
        auto frame = Eden::QuickOverlay::Read(); Eden::QuickOverlay::Hide();
        assert(!lifetime.expired() && frame.image); // an already captured frame keeps its pixels
    }
    assert(lifetime.expired() && !Eden::QuickOverlay::Read().image);
    std::vector<std::thread> decoders;
    for (unsigned n = 0; n < 4; ++n) decoders.emplace_back([&] {
        for (unsigned i = 0; i < 20; ++i) assert(load("portrait.png"));
    });
    for (auto& thread : decoders) thread.join();
    std::puts("Quick picture PASS: PNG bytes/dimensions/aggregate allocation limits, compressed bomb, "
              "malformed streams, alpha/aspect/orientation, no-follow paths, concurrent decode and 10000 atomic selections");
}
'''
with tempfile.TemporaryDirectory(prefix='eden-quick-picture-') as tmp:
    folder = Path(tmp)
    (folder / 'red.png').write_bytes(png(1, 1, b'\0\xff\0\0\xff'))
    (folder / 'blue.png').write_bytes(png(1, 1, b'\0\0\0\xff\xff'))
    rows = b''.join(b'\0' + (b'\xff\0\0\xff' if y < 4 else b'\0\0\xff\xff') * 4 for y in range(8))
    (folder / 'portrait.png').write_bytes(png(4, 8, rows))
    (folder / 'alpha.png').write_bytes(png(2, 1, b'\0\xff\0\0\xff\0\0\xff\0'))
    (folder / 'wide.png').write_bytes(png(1025, 1, b'\0' * (1025 * 4 + 1)))
    (folder / 'zero.png').write_bytes(png(0, 1, b'\0'))
    (folder / 'bomb.png').write_bytes(png(1, 1, b'\0' * (20 * 1024 * 1024)))
    (folder / 'bad.png').write_bytes(b'not a PNG' * 8)
    (folder / 'short.png').write_bytes((folder / 'red.png').read_bytes()[:40])
    with (folder / 'huge.png').open('wb') as file:
        file.truncate(4 * 1024 * 1024 + 1)
    (folder / 'alias.png').symlink_to(folder / 'red.png')
    (folder / 'alias').symlink_to(folder, target_is_directory=True)
    os.mkfifo(folder / 'fifo.png')
    (folder / 'fuzz').mkdir()
    randomizer = random.Random(1793121517661)
    valid = (folder / 'portrait.png').read_bytes()
    for n in range(160):
        if n < len(valid):
            data = valid[:n]
        else:
            data = bytearray(valid)
            for _ in range(8):
                data[randomizer.randrange(len(data))] ^= randomizer.randrange(1, 256)
        (folder / 'fuzz' / f'{n}.png').write_bytes(data)
    cpp, binary = folder / 'check.cpp', folder / 'check'
    cpp.write_text(code)
    for sanitizer in ('', 'address,undefined', 'thread'):
        flags = ['-fsanitize=' + sanitizer, '-fno-omit-frame-pointer'] if sanitizer else []
        subprocess.run([os.environ.get('CXX', 'clang++-18'), '-std=c++20', '-O1', '-g', '-UNDEBUG',
                        *flags, '-I', str(root / 'headless'), '-isystem', str(stb), str(cpp),
                        '-pthread', '-o', str(binary)], check=True)
        subprocess.run([str(binary), str(folder)], check=True, timeout=60)
