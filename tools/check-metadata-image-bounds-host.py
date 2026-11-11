#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Image policy adapted from ProsperoEden 20ef10e6ec2ffb6556fcfd4b61b3ed899f1db26b.
"""Run the actual metadata image functions with observable decoder/VFS calls.

Compile only in the existing Docker toolchain, with the checkout read-only and
scratch in /tmp. Decoder stand-ins prove rejection happens BEFORE pixel
allocation; an optional second fixture uses an explicitly supplied stb header.
Neither fixture is PS5 runtime qualification.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/metadata_bridge.cpp').read_text()
start = source.index('bool WriteTgaBytes(')
end = source.index('\nEden::NroAssets::Layout NroLayout(', start)
functions = source[start:end]
prefix = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace FileSys {
struct File {
    std::size_t size = 0;
    std::vector<unsigned char> bytes;
    unsigned reads = 0;
    std::size_t GetSize() const { return size; }
    std::vector<unsigned char> ReadBytes(std::size_t count) {
        ++reads;
        assert(count == size && count <= (64u << 20));
        return bytes;
    }
};
using VirtualFile = std::shared_ptr<File>;
}
std::vector<unsigned char> ReadFile(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
'''
fake_decoder = r'''
unsigned infos = 0, decodes = 0, frees = 0;
int info_width = 1, info_height = 1, decoded_width = 1, decoded_height = 1;
bool info_ok = true, decode_ok = true;
int stbi_info_from_memory(const unsigned char*, int size, int* w, int* h, int* c) {
    ++infos; assert(size > 0 && size <= (64 << 20));
    *w = info_width; *h = info_height; *c = 4; return info_ok;
}
unsigned char* stbi_load_from_memory(const unsigned char*, int, int* w, int* h, int* c, int required) {
    ++decodes; assert(required == 4);
    *w = decoded_width; *h = decoded_height; *c = 4;
    if (!decode_ok) return nullptr;
    // Invalid post-decode dimensions must be rejected without accessing pixels.
    const auto size = (*w <= 0 || *h <= 0 || *w > 4096 || *h > 4096) ? 4u :
                      static_cast<std::size_t>(*w) * *h * 4;
    auto* pixels = static_cast<unsigned char*>(std::malloc(size));
    assert(pixels);
    for (std::size_t i = 0; i < size; ++i) pixels[i] = static_cast<unsigned char>(i % 4 + 1);
    return pixels;
}
void stbi_image_free(void* pixels) { if (pixels) ++frees; std::free(pixels); }
void Reset(int width = 1, int height = 1) {
    infos = decodes = frees = 0;
    info_width = decoded_width = width; info_height = decoded_height = height;
    info_ok = decode_ok = true;
}
'''
fake_tests = r'''
int main() {
    constexpr std::size_t cap = 64u << 20;
    const unsigned char encoded[] = {42};
    const char* output = "cover.tga";
    Reset();
    assert(!WriteTga({}, output) && infos == 0 && decodes == 0);
    auto file = std::make_shared<FileSys::File>();
    file->size = cap + 1;
    assert(!WriteTga(file, output) && file->reads == 0 && infos == 0 && decodes == 0);
    file->size = std::numeric_limits<std::size_t>::max();
    assert(!WriteTga(file, output) && file->reads == 0 && infos == 0 && decodes == 0);
    file->size = 0;
    assert(!WriteTga(file, output) && file->reads == 0 && infos == 0 && decodes == 0);
    file->size = 2; file->bytes = {42};
    assert(!WriteTga(file, output) && file->reads == 1 && infos == 0 && decodes == 0);
    for (auto size : {cap + 1, std::numeric_limits<std::size_t>::max()}) {
        assert(!WriteTgaBytes(encoded, size, output, 512));
    }
    assert(!WriteTgaBytes(nullptr, 1, output, 512));
    assert(!WriteTgaBytes(encoded, 0, output, 512));
    assert(!WriteTgaBytes(encoded, 1, nullptr, 512));
    assert(!WriteTgaBytes(encoded, 1, output, 0));
    assert(infos == 0 && decodes == 0);
    Reset(); info_ok = false;
    assert(!WriteTgaBytes(encoded, 1, output, 512) && infos == 1 && decodes == 0);
    for (auto dimensions : {std::pair{0, 1}, {-1, 1}, {1, 0}, {1, -1},
                            {4097, 1}, {1, 4097}, {2147483647, 2147483647}}) {
        Reset(dimensions.first, dimensions.second);
        assert(!WriteTgaBytes(encoded, 1, output, 512) && infos == 1 && decodes == 0);
    }
    Reset(); decode_ok = false;
    assert(!WriteTgaBytes(encoded, 1, output, 512) && decodes == 1 && frees == 0);
    for (auto dimensions : {std::pair{0, 1}, {1, 0}, {4097, 1}, {1, 4097}}) {
        Reset(); decoded_width = dimensions.first; decoded_height = dimensions.second;
        assert(!WriteTgaBytes(encoded, 1, output, 512) && decodes == 1 && frees == 1);
    }
    Reset();
    assert(!WriteTgaBytes(encoded, 1, "absent/cover.tga", 512) && frees == 1);
    // Exactly the encoded cap is accepted without an int narrowing overflow.
    assert(WriteTgaBytes(encoded, cap, output, 4096));
    auto tga = ReadFile(output);
    assert(tga.size() == 22 && tga[12] == 1 && tga[14] == 1);
    assert(tga[18] == 3 && tga[19] == 2 && tga[20] == 1 && tga[21] == 4);
    Reset(); file->size = 1; file->bytes = {42};
    assert(WriteTga(file, output) && infos == 1 && decodes == 1 && frees == 1);
    // The 4096 boundary is legal; skinny images stress box-filter edge samples.
    for (auto dimensions : {std::pair{4096, 1}, {1, 4096}, {513, 1}, {1, 513}, {1, 1}}) {
        Reset(dimensions.first, dimensions.second);
        assert(WriteTgaBytes(encoded, 1, output, 512) && frees == 1);
        tga = ReadFile(output);
        const unsigned width = tga[12] + 256 * tga[13];
        const unsigned height = tga[14] + 256 * tga[15];
        assert(width > 0 && width <= 512 && height > 0 && height <= 512);
        assert(tga.size() == 18 + width * height * 4);
    }
}
'''
real_tests = r'''
int main() {
    // Uncompressed top-left TGA is also supported by the real stb decoder.
    std::vector<unsigned char> tga(22);
    tga[2] = 2; tga[12] = 1; tga[14] = 1; tga[16] = 32; tga[17] = 0x28;
    tga[18] = 3; tga[19] = 2; tga[20] = 1; tga[21] = 255;
    assert(WriteTgaBytes(tga.data(), tga.size(), "real.tga", 512));
    assert(ReadFile("real.tga") == tga);
    for (unsigned side : {0u, 4097u, 65535u}) {
        tga[12] = static_cast<unsigned char>(side);
        tga[13] = static_cast<unsigned char>(side >> 8);
        assert(!WriteTgaBytes(tga.data(), tga.size(), "real.tga", 512));
    }
    const unsigned char malformed[] = {0, 1, 2};
    assert(!WriteTgaBytes(malformed, sizeof(malformed), "real.tga", 512));
}
'''
compiler = os.environ.get('CXX') or next(
    (c for c in ('clang++-18', 'clang++', 'g++') if shutil.which(c)), None)
assert compiler, 'C++20 compiler required inside the Docker toolchain'
stb = Path(os.environ['STB_IMAGE_HEADER']) if os.environ.get('STB_IMAGE_HEADER') else None
if stb:
    # Never inspect the AI-restricted Eden source subtree, even for this optional fixture.
    assert '/vendor/sources/eden/' not in stb.absolute().as_posix(), 'restricted source subtree'
    assert stb.is_file(), 'STB_IMAGE_HEADER must name an explicitly supplied, permitted header'
fixtures = [('observed', prefix + fake_decoder + functions + fake_tests)]
if stb:
    fixtures.append(('stb', prefix + '#define STB_IMAGE_IMPLEMENTATION\n#include "' +
                     stb.as_posix() + '"\n' + functions + real_tests))
with tempfile.TemporaryDirectory(prefix='encore-metadata-images-') as tmp:
    for name, code in fixtures:
        for opt in ('-O0', '-O2'):
            exe = Path(tmp) / (name + opt)
            subprocess.run([compiler, '-std=c++20', opt, '-Wall', '-Wextra', '-Werror',
                            '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-x', 'c++', '-', '-o', str(exe)],
                           input=code, text=True, check=True)
            subprocess.run([str(exe)], cwd=tmp, check=True)
            print(f'Metadata image bounds: PASS {name} {opt} ASan/UBSan')
if not stb:
    print('NOTE real stb decoding not exercised; set STB_IMAGE_HEADER to a permitted header '
          'for that optional fixture. Observable no-read/no-decoder boundary tests passed')
