// ProsperoEden - Real builtin launcher/controller/render regression for Cover Flow.
// Synthetic game metadata/art only; no game payloads, console or network required.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_services.hpp"
#include "pe/ui/launcher.hpp"
#include "pe/ui/runtime_models.hpp"
#include "pe/core/file.hpp"
#include "pe/gfx/gl_program.hpp"
#include "settings_store.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using pe::ui::Key;
using pe::ui::Launcher;

struct Services : pe::host::FakeServices
{
    using FakeServices::FakeServices;
    std::vector<pe::ui::Game> data;
    std::uint64_t detail_id = 0;
    bool fail_save = false;
    int stale_reads = 0;
    bool load_image(const std::string &path, pe::gfx::Image *image) override
    {
        if (path.starts_with("obsolete-cover-")) ++stale_reads;
        return FakeServices::load_image(path, image);
    }
    std::vector<pe::ui::Game> games() override
    {
        auto out = data;
        std::erase_if(out, [this](const auto &game) { return !game.remote && !game_exists(game.file); });
        return out;
    }
    pe::ui::GameSettings game_settings(std::uint64_t id) override
    {
        detail_id = id;
        return FakeServices::game_settings(id);
    }
    bool set_preferences(const pe::ui::Preferences &p) override
    {
        return !fail_save && FakeServices::set_preferences(p);
    }
};

static void math_and_persistence(const std::string &out)
{
    using namespace pe::ui::cover_flow;
    Motion motion;
    for (int count : {0, 1, 2, 9, 100000})
    {
        motion.snap(0);
        for (int selected : {0, count / 2, std::max(0, count - 1), 0})
        {
            for (int frame = 0; frame < 120; ++frame)
            {
                motion.update(1.0f / 60, selected, count, false);
                assert(std::isfinite(motion.position));
                assert(motion.position >= 0 && motion.position <= std::max(0, count - 1));
                assert(std::fabs(motion.position - selected) <= 2.0f);
                int visible = 0;
                const auto order = pe::ui::cover_flow::order(count, motion.position, visible);
                assert(visible <= 9 && visible <= count);
                float previous = 1e9f;
                for (int n = 0; n < visible; ++n)
                {
                    const float distance = std::fabs(order[n] - motion.position);
                    assert(distance <= previous); previous = distance;
                }
            }
            assert(motion.position == selected);
        }
    }
    motion.update(1.0f / 60, 25, 50, true);
    assert(motion.position == 25);
    const pe::gfx::Rect stage{184, 152, 1056, 820};
    const auto center = project(0, stage), side = project(1, stage), far = project(3, stage);
    assert(center.quad.left_w == center.quad.right_w);
    assert(side.quad.left_w != side.quad.right_w); // real yaw/perspective, not a flat horizontal list
    assert(side.quad.left_top != side.quad.right_top);
    assert(side.depth < far.depth && side.bounds.h > far.bounds.h);
    for (int offset = -40; offset <= 40; ++offset)
    {
        const auto card = project(static_cast<float>(offset) / 10, stage);
        assert(card.bounds.w > 0 && card.quad.left_w > 0 && card.quad.right_w > 0);
    }

    const auto file = out + "/preferences.json";
    assert(Eden::LoadPreferences(file).library_view == "grid");
    Eden::Preferences prefs; prefs.library_view = "cover-flow";
    assert(Eden::SavePreferences(prefs, file));
    prefs = Eden::LoadPreferences(file);
    assert(prefs.library_view == "cover-flow");
    prefs.volume = 73;
    assert(Eden::SavePreferences(prefs, file));
    assert(Eden::LoadPreferences(file).library_view == "cover-flow");
    prefs.library_view = "invalid";
    assert(!Eden::SavePreferences(prefs, file));
    for (const auto &value : {"\"invalid\"", "42", "true", "null"})
    {
        std::ofstream(file) << "{\"launcher\":{\"library_view\":" << value << "}}";
        assert(Eden::LoadPreferences(file).library_view == "grid");
    }
    pe::ui::Preferences api;
    assert(nlohmann::json(api).at("library_view") == "grid");
    nlohmann::json patch = nlohmann::json(api); patch["library_view"] = "cover-flow";
    from_json(patch, api);
    assert(api.library_view == "cover-flow");
    from_json(nlohmann::json::object(), api);
    assert(api.library_view == "grid"); // old Lua contract remains valid
    std::cout << "PASS projection/depth, bounded motion/window, persistence/defaults/API\n";
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    std::cout.setf(std::ios::unitbuf);
    const std::string assets = argv[1], output = argv[2];
    const int width = std::stoi(argv[3]), height = std::stoi(argv[4]);
    std::filesystem::create_directories(output);
    math_and_persistence(output);
    auto platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto display = platform(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    EGLint major = 0, minor = 0;
    assert(eglInitialize(display, &major, &minor) && eglBindAPI(EGL_OPENGL_API));
    const EGLint attributes[]{EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 5,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    auto context = eglCreateContext(display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, attributes);
    assert(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context));
    pe::gfx::set_glsl_prefix("#version 450 core\n");
    pe::gfx::GlBatch batch; assert(batch.init());
    pe::gfx::Font font; std::string blob;
    assert(pe::read_file(assets + "/fonts/montserrat-medium.pefont", &blob) && font.load(blob));
    const auto font_texture = batch.create_font_texture(font);
    Services services(output);
    services.data = services.FakeServices::games();
    const auto originals = services.data;
    pe::ui::Textures textures(batch, services); assert(textures.load_art(assets));
    GLuint fbo = 0, image = 0;
    glGenFramebuffers(1, &fbo); glGenTextures(1, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    std::unique_ptr<Launcher> launcher;
    pe::gfx::DrawList list;
    auto tick = [&](int frames = 90) {
        for (int n = 0; n < frames; ++n)
        {
            launcher->update(1.0f / 60);
            // Let the real asynchronous game-list future run, as it does between console frames.
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    auto press = [&](Key key, int frames = 90) { launcher->press(key); tick(frames); };
    auto restart = [&](const std::string &view, bool calm = false, bool accessible = false, const char *theme = "canopy") {
        launcher.reset();
        auto prefs = services.preferences(); prefs.library_view = view; prefs.reduce_motion = calm;
        prefs.high_contrast = prefs.large_text = accessible; prefs.theme = theme;
        assert(services.set_preferences(prefs));
        launcher = std::make_unique<Launcher>(services, textures, pe::ui::Fonts{&font, font_texture}, false);
        textures.release();
        assert(textures.load_art(assets)); // the current theme's artwork, not the previous theme's
        textures.set_output_scale(static_cast<float>(width) / 1920.0f);
        tick(); press(pe::ui::th().top_nav ? Key::right : Key::r1);
    };
    auto render = [&](const std::string &name, bool flow) {
        for (int i = 0; i < 12; ++i) { list.clear(); launcher->draw(list); tick(1); }
        int projected = 0;
        for (const auto &instance : list.instances())
            if (instance.params[3] == 9 || instance.params[3] == 10) ++projected;
        assert(projected <= 18);
        assert(flow ? projected > 0 : projected == 0);
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        batch.sync_font_texture(font_texture, font);
        batch.draw(list, pe::gfx::fit_viewport(width, height), width, height);
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        assert(glGetError() == GL_NO_ERROR);
        stbi_flip_vertically_on_write(1);
        assert(stbi_write_png((output + "/" + name + ".png").c_str(), width, height, 4, pixels.data(), width * 4));
        std::cout << "FRAME " << name << " " << width << "x" << height << " projected=" << projected
                  << " instances=" << list.instances().size() << " calls=" << batch.last_draw_calls() << " GL=0\n";
    };
    auto selected = [&](int index) { assert(services.detail_id == services.data[static_cast<std::size_t>(index)].title_id); };

    restart("grid");
    render("grid-preserved", false);
    press(Key::r1); press(Key::r1); // Library -> Multiplayer -> Settings
    press(Key::down); press(Key::down);
    press(Key::right); press(Key::right); press(Key::right); // Appearance, tile 11
    press(Key::cross); press(Key::down); press(Key::down); press(Key::down);
    press(Key::right);
    assert(services.preferences().library_view == "cover-flow");
    render("cover-flow-setting", false);
    services.fail_save = true;
    press(Key::left);
    assert(services.preferences().library_view == "cover-flow");
    services.fail_save = false;
    press(Key::circle); // failed-save popup does not consume the intended Back
    launcher.reset();
    launcher = std::make_unique<Launcher>(services, textures, pe::ui::Fonts{&font, font_texture}, false);
    tick(); press(Key::r1); render("cover-flow-restored", true);
    std::cout << "PASS Settings controller toggle, save failure rollback and launcher recreation\n";
    restart("cover-flow");
    for (int i = 0; i < 600; ++i)
        textures.cover("obsolete-cover-" + std::to_string(i), 360.0f);
    press(Key::right, 1); // cancels the obsolete window rather than loading a long stale FIFO
    textures.pump(1.0f / 60);
    assert(services.stale_reads == 0);
    std::cout << "PASS rapid-selection cancellation of obsolete queued covers\n";
    restart("cover-flow");
    for (int i = 0; i < 5; ++i) press(Key::right, 1);
    tick(); selected(5);
    render("cover-flow", true);
    // Up/down do not skip; game options edit the exact selected game's existing settings.
    press(Key::down); press(Key::up); selected(5);
    const auto chosen = services.data[5];
    const bool docked = services.docked(chosen.title_id);
    press(Key::triangle); press(Key::right); press(Key::circle);
    assert(services.docked(chosen.title_id) != docked); selected(5);
    // Back to Home and into Library, no lost selection; reverse navigation is equally bounded.
    press(Key::circle); press(Key::r1); selected(5);
    for (int i = 0; i < 1000; ++i) launcher->press(Key::right);
    tick(); selected(static_cast<int>(services.data.size()) - 1);
    render("cover-flow-end", true);
    for (int i = 0; i < 7; ++i) press(Key::left, 1);
    tick();
    const int final = static_cast<int>(services.data.size()) - 8;
    selected(final);
    press(Key::cross);
    assert(launcher->selected_game() == services.game_path(services.data[static_cast<std::size_t>(final)].file));
    std::cout << "PASS real controller axis/bounds, rapid navigation/return, game options and launch\n";

    // Selection follows its file when another game disappears during a scan.
    restart("cover-flow"); press(Key::right); selected(1);
    services.removed.push_back(services.data[0].file); tick(180);
    assert(services.detail_id == services.data[1].title_id);
    services.removed.clear();

    services.data = {originals.front()}; services.data.front().cover.clear();
    services.data.front().name = "Missing cover - a long synthetic title for two-line accessibility checks";
    restart("cover-flow", true, true);
    for (int i = 0; i < 20; ++i) press(Key::right, 1);
    tick(); selected(0);
    render("cover-flow-one-fallback-calm-contrast-large", true);
    press(Key::cross); assert(launcher->selected_game() == services.game_path(services.data.front().file));
    services.data.clear(); restart("cover-flow");
    press(Key::cross); press(Key::triangle); press(Key::right);
    assert(launcher->selected_game().empty()); render("cover-flow-empty", false);

    services.data = originals;
    // Panel-style builtin themes use the same controller/data contract, rather than a flat grid.
    restart("cover-flow", false, false, "slate");
    for (int i = 0; i < 5; ++i) press(Key::right, 1);
    tick(); selected(5); render("cover-flow-panel", true);

    // The presentation must not invent parallel mod/download/homebrew launch paths.
    const auto modded = std::find_if(originals.begin(), originals.end(), [&](const auto &game) {
        return !services.mods(game.title_id).empty();
    });
    assert(modded != originals.end()); services.data = {*modded};
    restart("cover-flow");
    const bool mods_on = services.mods_enabled(modded->title_id);
    press(Key::square);
    assert(services.mods_enabled(modded->title_id) != mods_on);
    services.has_server = true;
    const auto with_remote = services.FakeServices::games();
    const auto remote = std::find_if(with_remote.begin(), with_remote.end(), [](const auto &game) { return game.remote; });
    assert(remote != with_remote.end()); services.data = {*remote};
    restart("cover-flow"); press(Key::square, 1);
    assert(!services.downloads().empty());
    press(Key::square, 1); assert(services.downloads().empty());
    press(Key::cross, 1); assert(!services.downloads().empty());
    assert(launcher->selected_game().empty()); // download first; never launch a remote file
    services.has_server = false;
    services.data = {originals.front()};
    services.data.front().file = "Synthetic homebrew.nro";
    services.data.front().title_id = 0;
    restart("cover-flow"); press(Key::cross);
    assert(launcher->selected_game() == services.game_path("Synthetic homebrew.nro"));
    std::cout << "PASS selected-game mods switch, remote queue/cancel/download-and-play, homebrew launch\n";

    // A very large library still records at most nine projected surfaces per frame.
    services.data = originals;
    for (int i = static_cast<int>(services.data.size()); i < 5000; ++i)
    {
        auto game = originals.front(); game.file = "Synthetic " + std::to_string(i) + ".nsp";
        game.title_id += static_cast<std::uint64_t>(i) * 65536; services.data.push_back(game);
    }
    restart("cover-flow", true);
    for (int i = 0; i < 4500; ++i) launcher->press(Key::right);
    tick(1); render("cover-flow-5000-calm", true);
    std::cout << "PASS empty/one/missing/5000 games, rescan identity, panel theme, reduced motion, large text/high contrast\n";
    launcher.reset(); textures.release(); batch.delete_texture(font_texture); batch.release();
    glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &image);
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(display, context); eglTerminate(display);
}
