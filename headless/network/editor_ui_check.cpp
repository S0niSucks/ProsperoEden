// SPDX-License-Identifier: GPL-3.0-or-later
// Real Launcher controller + real private Editor/NativeNetworkKeyboard, synthetic IME/server
// admission only. Optional screenshots use the actual baked font, draw list and Mesa renderer;
// no console/system-font fallback is exercised and no credential text is drawn or printed.
#include <bits/stdc++.h>
#include "host/fake_services.hpp"
#define private public
#include "pe/ui/launcher.hpp"
#undef private
#include "system_keyboard.h"
#include "pe/core/file.hpp"
#include "pe/gfx/gl_program.hpp"
#include "pe/gfx/system_fonts.hpp"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace {
using namespace Eden::Network;
using pe::ui::Key;
std::u16string next_text;
Eden::TextOutcome outcome = Eden::TextOutcome::accepted;
std::atomic<bool> wait_keyboard{false}, keyboard_entered{false};
Eden::TextRequest last_request;
class MenuServices final : public pe::host::FakeServices {
public:
    MenuServices(const std::string& root) : FakeServices(root + "/covers"), file(root + "/config/network-shares.json") {}
    Editor editor;
    std::string file;
    int connections = 0, saves = 0, removals = 0;
    bool blocked = false;
    std::atomic<bool> busy{false};
    struct Lease final : pe::ui::NetworkJob {
        std::atomic<bool>& busy;
        explicit Lease(std::atomic<bool>& busy) : busy(busy) {}
        ~Lease() override { busy = false; }
    };
    pe::ui::NetworkReservation reserve_network_job() override {
        if (blocked || busy.exchange(true)) return {};
        return std::make_unique<Lease>(busy);
    }
    bool network_job_busy() override { return busy; }
    EditorResult network_editor_open() override { return editor.Open(file); }
    EditorView network_editor_view() override { return editor.View(); }
    EditorResult network_editor_begin(const std::string& id) override { return blocked ? EditorResult::blocked : editor.Begin(id); }
    EditorResult network_editor_begin_named(const std::string& id, std::string name) override { return blocked ? EditorResult::blocked : editor.Begin(id, name); }
    EditorResult network_editor_change(Field field) override { return blocked ? EditorResult::blocked : editor.Change(field); }
    EditorResult network_editor_password(PasswordChoice choice) override { return blocked ? EditorResult::blocked : editor.Password(choice); }
    EditorResult network_editor_enter(Field field, const std::atomic<bool>& stop) override { return blocked ? EditorResult::blocked : editor.Enter(field, stop); }
    EditorResult network_editor_save(bool reconnect, const std::atomic<bool>& stop) override {
        if (blocked) return EditorResult::blocked;
        const auto result = editor.Save(stop);
        if (result == EditorResult::ok) { ++saves; connections += reconnect; }
        return result;
    }
    EditorResult network_editor_remove(const std::string& id, const std::atomic<bool>& stop) override {
        if (blocked) return EditorResult::blocked;
        const auto result = editor.Remove(id, stop); if (result == EditorResult::ok) ++removals; return result;
    }
    EditorResult network_editor_connection(bool connect) override { if (blocked) return EditorResult::blocked; connections += connect; return EditorResult::ok; }
    EditorResult network_enter_reserved(pe::ui::NetworkReservation job, Field field, const std::atomic<bool>& stop, KeyboardPrompts prompts) override {
        if (!job || blocked) return EditorResult::blocked;
        return editor.Enter(field, stop, MakeNativeNetworkKeyboard(std::move(prompts)));
    }
    EditorResult network_save_reserved(pe::ui::NetworkReservation job, bool reconnect, const std::atomic<bool>& stop) override {
        return job ? network_editor_save(reconnect, stop) : EditorResult::blocked;
    }
    EditorResult network_remove_reserved(pe::ui::NetworkReservation job, const std::string& id, const std::atomic<bool>& stop) override {
        return job ? network_editor_remove(id, stop) : EditorResult::blocked;
    }
    EditorResult network_connection_reserved(pe::ui::NetworkReservation job, bool connect, const std::atomic<bool>& stop) override {
        return !job ? EditorResult::blocked : stop ? EditorResult::cancelled : network_editor_connection(connect);
    }
    void network_editor_cancel() override { editor.Cancel(); }
};
void Pump(pe::ui::Launcher& menu) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (menu.network_job_.valid() && std::chrono::steady_clock::now() < deadline) {
        menu.update_network_shares(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(!menu.network_job_.valid());
}
void Focus(pe::ui::Launcher& menu, Field field) {
    const auto& rows = menu.network_view_.rows;
    const auto found = std::find_if(rows.begin(), rows.end(), [&](const auto& row) { return row.field == field; });
    assert(found != rows.end()); const auto target = found - rows.begin();
    while (menu.network_row_ < target) menu.press(Key::down);
    while (menu.network_row_ > target) menu.press(Key::up);
}
void Enter(pe::ui::Launcher& menu, Field field, std::u16string text) {
    Focus(menu, field); next_text = std::move(text); outcome = Eden::TextOutcome::accepted;
    menu.press(Key::cross); Pump(menu);
}
void Action(pe::ui::Launcher& menu, int index) {
    const int target = static_cast<int>(menu.network_view_.rows.size()) + index;
    while (menu.network_row_ < target) menu.press(Key::down);
    while (menu.network_row_ > target) menu.press(Key::up);
    menu.press(Key::cross); Pump(menu);
}
bool OpenGL() {
    auto get = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto display = get(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr); EGLint a{}, b{};
    if (!eglInitialize(display, &a, &b) || !eglBindAPI(EGL_OPENGL_API)) return false;
    const EGLint attributes[]{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    auto context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    return context != EGL_NO_CONTEXT && eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);
}
}
namespace Eden {
TextAnswer AskSystemKeyboard(const TextRequest& request, const std::atomic<bool>& stop) {
    last_request = request; keyboard_entered = true;
    while (wait_keyboard && !stop) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return {stop ? TextOutcome::cancelled : outcome, next_text};
}
}
namespace pe::gfx {
// The preview intentionally has no console fonts. Native baked-font Latin text/rendering is
// unchanged; requiring missing-script fallback is an assertion failure, not invented glyph data.
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
    assert(argc == 4);
    const std::string assets = argv[1], root = argv[2], output = argv[3];
    std::filesystem::create_directories(output);
    assert(OpenGL()); pe::gfx::set_glsl_prefix("#version 450 core\n");
    pe::gfx::Font font; std::string data;
    assert(pe::read_file(assets + "/fonts/montserrat-medium.pefont", &data) && font.load(data));
    pe::gfx::GlBatch batch; assert(batch.init());
    GLuint framebuffer{}, color{}; glGenFramebuffers(1, &framebuffer); glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color); glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1920, 1080);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    MenuServices services(root); pe::ui::Textures textures(batch, services);
    pe::ui::use_theme(services.preferences().theme); textures.load_art(assets);
    pe::ui::Launcher menu(services, textures, {&font, batch.create_font_texture(font)}, false);
    menu.finish_scan(true);
    const auto shoot = [&](const char* name, bool large = false) {
        pe::ui::look().large_text = large;
        for (int i = 0; i < 40; ++i) menu.update(1.0f / 60);
        pe::gfx::DrawList list; menu.draw(list); batch.sync_font_texture(menu.fonts_.texture, font);
        batch.draw(list, pe::gfx::fit_viewport(1920, 1080), 1920, 1080);
        std::vector<unsigned char> pixels(1920 * 1080 * 4); glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        stbi_flip_vertically_on_write(1);
        assert(stbi_write_png((output + "/" + name + ".png").c_str(), 1920, 1080, 4, pixels.data(), 1920 * 4));
        assert(glGetError() == GL_NO_ERROR);
        pe::ui::look().large_text = false;
    };
    pe::ui::set_fit_report([](std::string_view value, float, bool) {
        assert(value.find("Synthetic-only-password") == value.npos && value.find("Replacement-only-secret") == value.npos);
    });
    // Enter via actual Settings controller, after the preserved USB/Amiibo slots. The coordinator
    // owns the overflow-pagination fix needed to reach index 16; keep this assertion as a gate.
    menu.press(Key::l1); menu.press(Key::l1); // Home -> About -> Settings, as in the native preview
    for (auto key : {Key::down, Key::down, Key::down, Key::down}) menu.press(key);
    assert(menu.screen_ == pe::ui::Launcher::Screen::settings && menu.settings_.selected == 16);
    shoot("00-settings-network-entry");
    menu.press(Key::cross); assert(menu.modal_ == pe::ui::Launcher::Modal::network_shares);
    menu.press(Key::cross); assert(menu.network_view_.editing);
    Enter(menu, Field::name, u"Synthetic home NAS"); Enter(menu, Field::server, u"192.0.2.10");
    Enter(menu, Field::share, u"games"); Enter(menu, Field::folder, u"switch"); Enter(menu, Field::updates, u"addons");
    shoot("01-add-smb");
    Enter(menu, Field::user, u"fixture-user"); Enter(menu, Field::password, u"Synthetic-only-password");
    assert(last_request.password && last_request.initial.empty()); shoot("02-private-password-state");
    Focus(menu, Field::password); outcome = Eden::TextOutcome::cancelled; menu.press(Key::cross); Pump(menu);
    assert(last_request.initial.empty());
    Enter(menu, Field::password, u"Replacement-only-secret"); menu.press(Key::triangle); // restore saved (none for new)
    menu.press(Key::square); // explicit clear
    Enter(menu, Field::password, u"Synthetic-only-password");
    Action(menu, 0); assert(services.saves == 1 && services.connections == 0);
    auto config = ReadConfiguration(services.file); const auto id = config.shares[0].id;
    assert(config.shares[0].password == "Synthetic-only-password");
    menu.press(Key::cross); Enter(menu, Field::name, u"Discard this name");
    menu.press(Key::circle); assert(menu.message_.empty()); shoot("03-discard-confirm"); menu.press(Key::circle); assert(menu.network_view_.editing);
    menu.press(Key::circle); menu.press(Key::cross); assert(!menu.network_view_.editing);
    assert(ReadConfiguration(services.file).shares[0].name == "Synthetic home NAS");
    menu.press(Key::cross); Enter(menu, Field::password, u"Replacement-only-secret"); menu.press(Key::triangle);
    Action(menu, 1); assert(services.connections == 1 && ReadConfiguration(services.file).shares[0].password == "Synthetic-only-password");
    menu.press(Key::down); menu.press(Key::cross); // Add second location
    Focus(menu, Field::protocol); menu.press(Key::right); Enter(menu, Field::server, u"192.0.2.11"); Enter(menu, Field::share, u"/exports/games");
    Enter(menu, Field::uid, u"1001"); Enter(menu, Field::gid, u"1002"); Enter(menu, Field::mount_port, u"20048"); shoot("04-nfs-options");
    while (menu.network_row_ < static_cast<int>(menu.network_view_.rows.size())) menu.press(Key::down);
    shoot("05-save-choice"); Action(menu, 0); assert(services.connections == 1);
    menu.press(Key::square); assert(menu.message_.empty()); shoot("06-delete-confirm"); menu.press(Key::circle); assert(services.removals == 0);
    menu.press(Key::square); menu.press(Key::cross); Pump(menu); assert(services.removals == 1);
    assert(ReadConfiguration(services.file).shares[0].protocol == "nfs");
    // Backend admission failure stays actionable and never writes or contacts anything.
    services.blocked = true; menu.press(Key::cross); assert(!menu.network_view_.editing); shoot("07-admission-blocked"); services.blocked = false;
    // Invalid import: no blanket reset, no leaked parser snippet.
    std::ofstream(services.file) << "{broken-private-import";
    menu.press(Key::circle); menu.open_modal(pe::ui::Launcher::Modal::network_shares); menu.press(Key::cross);
    assert(!menu.network_view_.editing); shoot("08-invalid-import", true);
    std::ofstream(services.file) << "{\"shares\":[]}";
    menu.press(Key::circle); menu.open_modal(pe::ui::Launcher::Modal::network_shares); menu.press(Key::cross);
    // Actual Launcher teardown cancels and joins an outstanding private keyboard future.
    {
        pe::ui::Launcher pending(services, textures, {}, false);
        pending.open_modal(pe::ui::Launcher::Modal::network_shares); pending.press(Key::cross);
        wait_keyboard = true; keyboard_entered = false; Focus(pending, Field::password); pending.press(Key::cross);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!keyboard_entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        assert(keyboard_entered);
    }
    wait_keyboard = false;
    std::cout << "actual Settings/Launcher + private native keyboard provider + real Editor: add/edit/cancel/delete confirmations, masked keep/replace/clear, opt-in reconnect, blocked admission, invalid import, async teardown and 9 synthetic Mesa previews PASS\n";
}
