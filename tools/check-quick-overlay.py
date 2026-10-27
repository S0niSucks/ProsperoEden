#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render the actual OpenGL HUD method on Mesa, with the quick strip's bounded text/font."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/graphics.cpp').read_text()
start = source.index('    void DrawHud(')
method = source[start:source.index('    unsigned presented_frames', start)]
code = r'''
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
#include "hud.h"
#include "loading_text_glsl.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tools/launcher/stb/stb_image_write.h"
template<class F> struct ExitGuard { F f; ~ExitGuard() { f(); } };
struct Exit { template<class F> auto operator+(F f) { return ExitGuard<F>{f}; } };
#define SCOPE_EXIT auto restore = Exit{} + [&]()
using namespace Eden;
std::atomic<int> hud_size{2}, hud_corner{3}; // quick strip must ignore counter preferences
std::atomic<bool> hud_colors{true};
void Check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "%s\n", message); std::abort(); } }
void Cleanup(bool ok, const char* message) { Check(ok, message); }
struct Preview {
    bool owned = true;
    bool* window_owner = &owned;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context, hud_context;
    GLuint hud_program{}, hud_vao{};
    GLuint hud_picture_texture{};
    std::shared_ptr<const QuickPicture> hud_picture;
''' + method + r'''
};
int main(int argc, char** argv) {
    assert(argc == 2);
    auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    const EGLDisplay display = get(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major, minor; Check(eglInitialize(display, &major, &minor), "EGL initialize");
    Check(eglBindAPI(EGL_OPENGL_API), "EGL OpenGL");
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config; EGLint count;
    Check(eglChooseConfig(display, attributes, &config, 1, &count) && count, "EGL config");
    const EGLint ctx_attributes[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    const auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attributes);
    assert(context != EGL_NO_CONTEXT);
    const char* lines[] = {
        "AMIIBO 2/3 Mario.bin | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats",
        "AMIIBO No .bin files in amiibo/ | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats",
        "CHEATS 1/2 OFF Infinite stamina | L/R | CROSS Toggle | CIRCLE Close | UP/DOWN Amiibo | This session",
        "AMIIBO 1/2 Mario - Smash | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats",
        "AMIIBO 2/2 Mario - Second | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats",
        "AMIIBO 1/2 Missing picture | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats"};
    auto portrait = std::make_shared<QuickPicture>(), blue = std::make_shared<QuickPicture>();
    for (unsigned y = 0; y < 256; ++y) for (unsigned x = 64; x < 192; ++x) {
        auto* p = portrait->rgba.data() + (y * 256 + x) * 4;
        p[y < 128 ? 0 : 2] = 255; p[3] = 255;
        auto* b = blue->rgba.data() + (y * 256 + x) * 4; b[2] = 255; b[3] = 255;
    }
    std::filesystem::create_directories(argv[1]);
    for (int width : {1280, 1920}) {
        const int height = width * 9 / 16;
        const EGLint dimensions[] = {EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE};
        const auto surface = eglCreatePbufferSurface(display, config, dimensions);
        Check(eglMakeCurrent(display, surface, surface, context), "EGL current");
        Preview preview; preview.display = display; preview.surface = surface;
        preview.context = preview.hud_context = context;
        for (unsigned n = 0; n < std::size(lines); ++n) {
            QuickOverlay::Show(lines[n], n == 3 ? portrait : n == 4 ? blue : nullptr);
            const auto frame = QuickOverlay::Read(); const auto text = frame.text;
            const auto packed = MakeQuickHudSnapshot(text.data());
            assert(packed.x == kHudBoxX && packed.y == kHudBoxY);
            assert(packed.width + 2 * packed.x <= kHudPictureWidth);
            glClearColor(0.08f, 0.12f, 0.18f, 1); glClear(GL_COLOR_BUFFER_BIT);
            preview.DrawHud(text.data(), false, true, frame.image); assert(glGetError() == GL_NO_ERROR);
            std::vector<unsigned char> pixels(width * height * 3);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            // Reject an invisible strip: there must be actual bright glyph pixels near its top.
            unsigned ink = 0;
            for (size_t i = 0; i < pixels.size(); i += 3)
                ink += pixels[i] > 150 && pixels[i + 1] > 150 && pixels[i + 2] > 150;
            assert(ink > 100);
            if (n >= 3) {
                const auto units = [height](float value) { return static_cast<int>(std::lround(value * height / 1080.0f)); };
                const int top = units(kHudBoxY) + units(kHudBoxUnits * (1 + 0.5f * QuickHudSize(text.data()))) + units(QuickPicture::kGap);
                const auto sample = [&](int reference_y) {
                    return pixels.data() + ((height - top - units(reference_y) - 1) * width + units(kHudBoxX + QuickPicture::kBox / 2)) * 3;
                };
                const auto* upper = sample(40); const auto* lower = sample(184);
                if (n == 3) assert(upper[0] > 240 && upper[2] < 10 && lower[2] > 240 && lower[0] < 10);
                if (n == 4) assert(upper[2] > 240 && upper[0] < 10 && lower[2] > 240);
                if (n == 5) assert(upper[0] < 25 && upper[1] < 35 && upper[2] < 50); // no stale photo/panel
            }
            stbi_flip_vertically_on_write(1);
            const auto file = std::filesystem::path(argv[1]) / (std::to_string(width) + "-quick-" + std::to_string(n) + ".png");
            assert(stbi_write_png(file.c_str(), width, height, 3, pixels.data(), width * 3));
        }
        glDeleteProgram(preview.hud_program); glDeleteVertexArrays(1, &preview.hud_vao);
        glDeleteTextures(1, &preview.hud_picture_texture);
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroySurface(display, surface);
    }
    eglDestroyContext(display, context); eglTerminate(display);
    std::puts("Quick overlay PASS: actual DrawHud/font/photo, Mesa GLSL, bounded chip, alpha/orientation, image replacement and missing-photo fallback at 720p/1080p");
}
'''
with tempfile.TemporaryDirectory(prefix='eden-quick-overlay-') as tmp:
    cpp, binary = Path(tmp) / 'preview.cpp', Path(tmp) / 'preview'
    cpp.write_text(code)
    (Path(tmp) / 'loading_text_glsl.h').write_text(
        'inline constexpr const char* kLoadingTextGlsl = R"glsl(\n' +
        (root / 'headless/loading_text.glsl').read_text() + ')glsl";\n')
    subprocess.run(['c++', '-std=c++20', '-O2', '-D_GLIBCXX_ASSERTIONS=1', '-I', str(root),
                    '-I', str(root / 'headless'), str(cpp), '-lEGL', '-lGL', '-pthread', '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(Path(sys.argv[1]).resolve())], check=True,
                   env={**os.environ, 'LIBGL_ALWAYS_SOFTWARE': '1'})
