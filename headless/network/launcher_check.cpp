// SPDX-License-Identifier: GPL-3.0-or-later
// Actual native Launcher lifecycle/list-selection/error consumer with a service seam (not
// EdenServices) supplying real NAS-derived metadata. Graphics/console services are inert.
#include <bits/stdc++.h>
#include "host/fake_services.hpp"
#define private public
#include "pe/ui/launcher.hpp"
#undef private
#include "metadata.h"
#include "vfs.h"
#include "settings_store.h"

namespace {
class MenuServices final : public pe::host::FakeServices {
public:
    MenuServices(const std::string& root, Eden::Network::Metadata metadata, std::string failure = {})
        : FakeServices(root + "/ui-covers"), metadata(std::move(metadata)), failure(std::move(failure)) {}
    ~MenuServices() override { Eden::Network::StopDiscovery(); }
    pe::ui::Home home() override {
        pe::ui::Home h; h.setup_ready = h.homebrew_ready = true;
        h.last_file = uri; h.last_title_id = metadata.title_id;
        h.last_exists = Eden::Network::Known(uri); h.last_title = metadata.title;
        h.launch_failed = !failure.empty(); h.status = failure;
        h.recents.push_back({uri, metadata.title, {}});
        return h;
    }
    std::vector<pe::ui::Game> games() override {
        pe::ui::Game g; g.file = uri; g.name = metadata.title; g.title_id = metadata.title_id; g.network = true;
        pe::ui::Game local; local.file = "Local.nsp"; local.name = "Local fixture";
        return {g, local};
    }
    bool game_exists(const std::string& file) override { return file == "Local.nsp" || Eden::Network::Known(file); }
    std::string game_path(const std::string& file) override { return Eden::Network::Known(file) ? file : std::string{}; }
    void will_play(const std::string& file) override { played = file; }
    std::vector<pe::ui::Mod> mods(std::uint64_t) override { return {}; }
    pe::ui::GameSettings game_settings(std::uint64_t title) override { settings_title = title; return {}; }
    Eden::Network::Metadata metadata;
    std::string failure, played;
    std::uint64_t settings_title = 0;
    const std::string uri = "pe-net://test/Demo.NRO";
};
}

// No GL context, font rasterizer or theme-kit rendering is initialized in a controller test.
namespace pe::gfx {
GlBatch::~GlBatch() = default;
bool Font::can_draw(std::string_view) const { return true; }
std::string Font::drawable(std::string_view text) const { return std::string{text}; }
}
namespace pe::ui {
Textures::~Textures() = default;
void Textures::load_wallpaper(const std::string&) {}
void Textures::clear_wallpaper() {}
void Textures::load_game_backdrop(const std::string&) {}
void Textures::clear_game_backdrop() {}
// This renderer fake has no queued covers; it does not qualify real cover cancellation.
void Textures::discard_pending_covers() {}
bool theme_from_kit(std::string_view, Theme*) { return false; }
}

void NetworkLauncherCheck(const std::string& root, const Eden::Network::Metadata& metadata) {
    auto consumer = Eden::Network::OpenVfs("pe-net://test/Demo.NRO"); assert(consumer);
    {
        MenuServices services(root, metadata);
        pe::gfx::GlBatch batch;
        pe::ui::Textures textures(batch, services);
        pe::ui::Launcher menu(services, textures, {}, false);
        menu.finish_scan(true);
        assert(menu.games_.at(0).title_id == metadata.title_id);
        assert(menu.home_.last_file == services.uri && menu.home_.recents.at(0).file == services.uri);
        auto reordered = services.games(); std::reverse(reordered.begin(), reordered.end());
        menu.apply_games(std::move(reordered));
        assert(menu.games_.at(menu.library_.selected).file == services.uri);
        menu.read_selected_details(); assert(services.settings_title == metadata.title_id);
        menu.start_game(services.uri, metadata.title, {});
        assert(menu.selected_game() == services.uri && services.played == services.uri);
    } // actual Launcher destructor joins its async scan; backend drops discovery, not consumers
    unsigned char magic[4]{};
    assert(consumer->Read(magic, sizeof(magic), 0x10) == sizeof(magic) && std::memcmp(magic, "NRO0", 4) == 0);
}

void NetworkLauncherFailureCheck(const std::string& root, const std::string& failure) {
    assert(!failure.empty());
    MenuServices services(root, {}, failure);
    pe::gfx::GlBatch batch;
    pe::ui::Textures textures(batch, services);
    pe::ui::Launcher menu(services, textures, {}, false);
    menu.finish_scan(true);
    assert(menu.selected_game().empty() && menu.home_.launch_failed && menu.home_.status == failure);
    const auto cues = menu.take_cues();
    assert(std::find(cues.begin(), cues.end(), pe::audio::Cue::notify) != cues.end());
}
