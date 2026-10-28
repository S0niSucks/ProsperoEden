// SPDX-License-Identifier: GPL-3.0-or-later
// Actual native Launcher controller/draw list/Mesa, synthetic services only; no provider or IME.
#include <bits/stdc++.h>
#include "host/fake_services.hpp"
#define private public
#include "pe/ui/launcher.hpp"
#undef private
#include "pe/core/file.hpp"
#include "pe/gfx/gl_program.hpp"
#include "pe/gfx/system_fonts.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
using pe::ui::Key;
using Eden::Nextendo::Action;
using Eden::Nextendo::Status;
class MenuServices final : public pe::host::FakeServices {
public:
    explicit MenuServices(const std::string& root) : FakeServices(root) {}
    std::atomic<unsigned> calls{0}, links{0}, legacy_links{0};
    std::atomic<bool> hold{false};
    std::string captured_profile;
    const std::atomic<bool>* captured_stop = nullptr;
    bool captured_consent = false;
    Status nextendo_action(const std::string&, Action action, const std::atomic<bool>&) override {
        if (action == Action::link_browser) ++legacy_links;
        return {false, false, false, 0, Eden::Nextendo::GameLinkConsentRequired};
    }
    Status nextendo_action_with_consent(const std::string& profile, Action action,
        const std::atomic<bool>& stop, bool confirmed) override {
        captured_profile = profile; captured_stop = &stop; captured_consent = confirmed;
        ++calls;
        if (action == Action::link_browser) { assert(confirmed); ++links; }
        else assert(!confirmed);
        while (hold.load()) std::this_thread::sleep_for(std::chrono::milliseconds{1});
        if (stop.load()) return {false, false, false, 0, "Nextendo action cancelled."};
        return {true, action == Action::enable, false, 1,
            action == Action::check_account ? Eden::Nextendo::GameCheckUnavailable : ""};
    }
};
class UnapprovedServices final : public pe::host::FakeServices {
public:
    using FakeServices::FakeServices;
    unsigned calls = 0;
    Status nextendo_action(const std::string&, Action, const std::atomic<bool>&) override { ++calls; return {}; }
};
template<class F> void Until(F ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{5};
    while (!ready() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds{1});
    assert(ready());
}
void Pump(pe::ui::Launcher& menu) {
    Until([&] { menu.update_multiplayer(); return !menu.nextendo_request_.valid(); });
}
void BrowserRow(pe::ui::Launcher& menu) {
    if (menu.room_page_ != pe::ui::Launcher::RoomPage::nextendo) {
        menu.room_row_ = 2; menu.press(Key::cross);
    }
    while (menu.room_row_ < 4) menu.press(Key::down);
}
}

// Same baked-font-only host seam as the existing native UI checks; no system font/IME path.
namespace pe::gfx {
struct SystemFonts::Face {};
SystemFonts::SystemFonts() = default;
SystemFonts::~SystemFonts() = default;
void SystemFonts::set(std::vector<std::string>, std::string_view) { assert(false); }
int SystemFonts::face_for(char32_t) { return -1; }
bool SystemFonts::face_has(int, char32_t) { return false; }
void SystemFonts::shape(int, const char32_t*, int, bool, std::vector<RunGlyph>*) { assert(false); }
bool SystemFonts::field(int, std::uint32_t, float, float, GlyphField*) { assert(false); return false; }
std::string SystemFonts::loaded() const { return {}; }
}
namespace pe::ui { bool theme_from_kit(std::string_view, Theme*) { return false; } }

int main(int argc, char** argv) {
    assert(argc == 6);
    const std::string assets = argv[1], root = argv[2], output = argv[3];
    const int width = std::stoi(argv[4]), height = std::stoi(argv[5]);
    std::filesystem::create_directories(output);
    const auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    assert(get);
    auto display = get(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint a{}, b{};
    assert(eglInitialize(display, &a, &b) && eglBindAPI(EGL_OPENGL_API));
    const EGLint attributes[]{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    auto context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    assert(context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    pe::gfx::set_glsl_prefix("#version 450 core\n");
    pe::gfx::Font font; std::string data;
    assert(pe::read_file(assets + "/fonts/montserrat-medium.pefont", &data) && font.load(data));
    pe::gfx::GlBatch batch; assert(batch.init());
    GLuint framebuffer{}, color{}; glGenFramebuffers(1, &framebuffer); glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color); glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    {
        UnapprovedServices unapproved(root);
        const std::atomic<bool> stop{false};
        // Legacy/Lua-style frontends cannot gain linking simply by accepting the new base method.
        assert(unapproved.nextendo_action_with_consent("synthetic", Action::link_browser, stop, false).error == Eden::Nextendo::GameLinkConsentRequired);
        assert(unapproved.nextendo_action_with_consent("synthetic", Action::link_browser, stop, true).error == Eden::Nextendo::GameLinkConsentRequired);
        assert(unapproved.calls == 0);
        MenuServices services(root); services.people = {{"First", true}, {"Second", false}};
        pe::ui::Textures textures(batch, services); textures.load_art(assets);
        pe::ui::Launcher menu(services, textures, {&font, batch.create_font_texture(font)}, false);
        menu.finish_scan(true);
        pe::ui::set_fit_report([](std::string_view text, float, bool clipped) {
            assert(text.find("private-synthetic-marker") == text.npos);
            if (text == Eden::Nextendo::GameLinkConsentMessage) assert(!clipped);
        });
        const auto shoot = [&](const char* name, bool large = false) {
            pe::ui::look().large_text = large;
            for (int i = 0; i < 50; ++i) menu.update(1.0f / 60);
            pe::gfx::DrawList list; menu.draw(list);
            batch.sync_font_texture(menu.fonts_.texture, font);
            batch.draw(list, pe::gfx::fit_viewport(width, height), width, height);
            std::vector<unsigned char> pixels(width * height * 4);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            assert(std::any_of(pixels.begin(), pixels.end(), [](unsigned char c) { return c > 0 && c < 255; }));
            stbi_flip_vertically_on_write(1);
            assert(stbi_write_png((output + "/" + name + ".png").c_str(), width, height, 4, pixels.data(), width * 4));
            assert(glGetError() == GL_NO_ERROR);
            pe::ui::look().large_text = false;
        };
        menu.press(Key::r1); menu.press(Key::r1); BrowserRow(menu);
        assert(!menu.nextendo_status_.linked && services.calls == 0);
        menu.nextendo_browser_url_ = "http://127.0.0.1:32123/start"; // cannot count as consent
        menu.press(Key::cross); assert(menu.nextendo_browser_confirm_ && services.calls == 0);
        menu.press(Key::cross); assert(!menu.nextendo_request_.valid()); // description not drawn yet
        { pe::gfx::DrawList hidden; menu.draw(hidden); }
        assert(!menu.nextendo_browser_confirm_.shown); // fade/transition is not presented consent
        menu.press(Key::cross); assert(!menu.nextendo_request_.valid());
        shoot("01-first-link-consent"); assert(menu.nextendo_browser_confirm_.shown);
        shoot("02-consent-large", true);
        menu.press(Key::up); menu.press(Key::down); menu.press(Key::cross);
        assert(services.calls == 0 && !menu.nextendo_browser_confirm_.shown); // moving cancels
        shoot("03-focus-reconfirmation");
        menu.press(Key::circle); assert(!menu.nextendo_browser_confirm_ && !menu.nextendo_request_.valid());
        assert(!menu.nextendo_browser_confirm_.shown && menu.nextendo_browser_confirm_.profile.empty());
        BrowserRow(menu); menu.press(Key::cross); assert(services.calls == 0);
        shoot("04-cancel-reconfirmation");
        services.people[0].playing = false; services.people[1].playing = true;
        menu.press(Key::cross); assert(services.calls == 0 && !menu.nextendo_browser_confirm_.shown);
        shoot("05-profile-reconfirmation");
        const auto bound = services.profile_key(1);
        services.hold = true; menu.press(Key::cross); Until([&] { return services.calls.load() == 1; });
        assert(!menu.nextendo_browser_confirm_ && menu.nextendo_browser_confirm_.profile.empty());
        assert(services.captured_profile == bound && services.captured_stop == &menu.nextendo_stop_ && services.captured_consent);
        menu.press(Key::cross); assert(services.calls == 1); // job owns this single request
        menu.nextendo_browser_url_ = "http://127.0.0.1:32123/start?private-synthetic-marker=" + std::string(9000, 'x');
        shoot("06-private-url-refused");
        menu.press(Key::circle); services.hold = false; Pump(menu);
        assert(menu.nextendo_status_.error == "Nextendo action cancelled." && services.links == 1);
        BrowserRow(menu); menu.press(Key::cross); assert(services.calls == 1);
        shoot("07-fresh-consent-after-cancel"); menu.press(Key::cross); Pump(menu);
        assert(services.calls == 2 && services.links == 2 && menu.nextendo_status_.linked && !menu.nextendo_status_.enabled);
        shoot("08-linked-default-off");
        menu.press(Key::cross); assert(services.calls == 2); // replacement also requires consent
        shoot("09-replacement-consent");
        menu.press(Key::up); menu.press(Key::up); menu.press(Key::up); menu.press(Key::cross); Pump(menu);
        assert(menu.nextendo_status_.enabled && !services.captured_consent && services.legacy_links == 0);
        menu.press(Key::down); menu.press(Key::cross); Pump(menu);
        assert(!menu.nextendo_status_.checked && menu.nextendo_status_.error == Eden::Nextendo::GameCheckUnavailable);
        shoot("10-game-only-check");
        menu.nextendo_status_.error = "private-synthetic-marker" + std::string(9000, 'x');
        shoot("11-private-diagnostic-refused");
        // Leaving/reentering a section also clears consent via the actual multiplayer controller.
        BrowserRow(menu); menu.press(Key::cross); shoot("12-before-section-change");
        menu.press(Key::circle); menu.press(Key::r1);
        assert(menu.screen_ == pe::ui::Launcher::Screen::settings);
        menu.press(Key::l1); BrowserRow(menu);
        const auto calls = services.calls.load(); menu.press(Key::cross);
        assert(services.calls == calls && !menu.nextendo_request_.valid() && !menu.nextendo_browser_confirm_.shown);
        pe::ui::set_fit_report(nullptr);
    }
    glDeleteRenderbuffers(1, &color); glDeleteFramebuffers(1, &framebuffer); batch.release();
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context); eglTerminate(display);
    std::cout << "Native Nextendo consent UI PASS: first/replacement confirmation, rendered description, focus/cancel/profile resets, one captured profile+stop job, inert unapproved frontend, default-off/explicit enable, safe errors/URL at " << width << 'x' << height << '\n';
}
