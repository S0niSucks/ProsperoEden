// SPDX-License-Identifier: GPL-3.0-or-later
// Complete extracted load_image, real Image, real image.cpp, real host stb JPEG.
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include <stb_image.h>
#include "pe/gfx/image.hpp"
#include "user_content.h"

static_assert(!Eden::UserContent::kIoAvailable, "gate must remain absent/zero");

namespace witness {
struct State {
    bool fail_assign = false, armed = false, live = false;
    int loads = 0, frees = 0, null_frees = 0, unexpected_frees = 0, injected = 0;
    int decoded_width = 0, decoded_height = 0;
    std::size_t injected_size = 0;
    void* pixels = nullptr;
} state;
int controls = 0, regressions = 0, fault_hits = 0;
void require(bool value, const char* what) {
    if (!value) { ++controls; std::fprintf(stderr, "CONTROL_FAILURE %s\n", what); }
}
void* allocate(std::size_t size) {
    if (state.armed) {
        state.armed = false;
        ++state.injected;
        state.injected_size = size;
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
} // namespace witness

void* operator new(std::size_t n) { return witness::allocate(n); }
void* operator new[](std::size_t n) { return witness::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

extern "C" stbi_uc* __real_stbi_load(const char*, int*, int*, int*, int);
extern "C" void __real_stbi_image_free(void*);
extern "C" stbi_uc* __wrap_stbi_load(const char* path, int* w, int* h, int* c, int requested) {
    auto* pixels = __real_stbi_load(path, w, h, c, requested);
    auto& s = witness::state;
    ++s.loads;
    s.pixels = pixels;
    s.live = pixels != nullptr;
    if (pixels) { s.decoded_width = *w; s.decoded_height = *h; }
    // Arm AFTER actual codec allocation/return, BEFORE actual vector::assign.
    if (pixels && s.fail_assign) s.armed = true;
    return pixels;
}
extern "C" void __wrap_stbi_image_free(void* p) {
    auto& s = witness::state;
    if (!p) { ++s.null_frees; __real_stbi_image_free(p); return; }
    if (p != s.pixels || !s.live) { ++s.unexpected_frees; return; }
    ++s.frees;
    s.live = false;
    __real_stbi_image_free(p);
}

namespace Eden {
// Fail-stop seams for UNEXERCISED wallpaper/relative-app routes. They neither
// return success nor replace JPEG acquisition, decode, assign or ownership.
bool ValidAssetsDir(const std::string&) { std::abort(); }
std::string AppFile(const std::string&) { std::abort(); }
}
class EdenServices {
  public:
    bool load_image(const std::string&, pe::gfx::Image*);
};

#include "load_image.inc"

static void cleanup_leak() {
    auto& s = witness::state;
    if (s.live) { __real_stbi_image_free(s.pixels); s.live = false; } // after observation
}

static void allocation_case(EdenServices& services, const std::string& file, int iteration) {
    using namespace witness;
    state = {};
    state.fail_assign = true;
    pe::gfx::Image image;
    bool threw = false;
    try { (void)services.load_image(file, &image); }
    catch (const std::bad_alloc&) { threw = true; }
    catch (...) { require(false, "unexpected exception type"); }
    state.armed = false;
    require(state.loads == 1 && state.pixels, "real JPEG decoded, not TGA/DDS shortcut");
    require(threw && state.injected == 1 && state.injected_size == 16,
            "actual 2x2 RGBA vector::assign allocation failed");
    require(image.width == 2 && image.height == 2 && image.rgba.empty(), "assign was reached");
    require(state.null_frees == 0 && state.unexpected_frees == 0, "no null/duplicate/other-pointer free");
    fault_hits += state.injected;
    const bool leaked = state.live && state.frees == 0;
    if (leaked) ++regressions;
    else require(!state.live && state.frees == 1, "one actual stbi_image_free after unwind");
    std::printf("JPEG_CASE iteration=%d free_calls=%d leaked=%d\n", iteration, state.frees, leaked);
    cleanup_leak();
}

static void codec_control(EdenServices& services, const std::string& path, int mode) {
    using namespace witness;
    state = {};
    pe::gfx::Image image;
    const bool ok = services.load_image(path, &image);
    require(state.loads == 1 && !state.injected, "real codec path, no fault on control");
    require(ok == (mode == 0), "success/rejected dimensions/invalid JPEG return value");
    require(state.frees == (mode == 2 ? 0 : 1) && !state.live,
            "one free for valid pointer, zero for null");
    require(state.null_frees == 0 && state.unexpected_frees == 0, "no spurious deleter calls");
    if (mode == 0) {
        require(image.width == 2 && image.height == 2 && image.rgba.size() == 16, "real decoded output");
        bool correct = image.rgba.size() == 16;
        for (std::size_t i = 0; i < image.rgba.size(); ++i)
            correct = correct && image.rgba[i] == (i % 4 == 3 ? 255 : 128);
        require(correct, "synthetic JPEG pixels decoded, not fabricated success");
    } else if (mode == 1) {
        require(state.pixels != nullptr && image.rgba.empty(), "real 8193x1 decode rejected before copy");
        require(state.decoded_width == 8193 && state.decoded_height == 1, "actual oversized dimensions");
    } else require(state.pixels == nullptr && image.rgba.empty(), "invalid stream returns null");
    cleanup_leak();
}

int main(int argc, char** argv) {
    using namespace witness;
    if (argc != 4 || argv[1][0] != '/' || argv[2][0] != '/' || argv[3][0] != '/') return 20;
    const std::string tiny = argv[1], wide = argv[2], invalid = argv[3];
    EdenServices services;
    for (int n = 0; n < 8; ++n) allocation_case(services, tiny, n);
    codec_control(services, tiny, 0);
    codec_control(services, wide, 1);
    codec_control(services, invalid, 2);
    require(fault_hits == 8, "all eight assign allocation failures were injected");
    std::printf("SUMMARY kind=jpeg controls=%d regressions=%d fault_hits=%d\n",
                controls, regressions, fault_hits);
    return controls ? 20 : regressions ? 10 : 0;
}
