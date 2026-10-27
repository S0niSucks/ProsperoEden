// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "file_amiibo.h"
#include "amiibo_catalog.h"
#include "quick_overlay.h"
#include "live_cheats.h"
#include "ps5_pad.hpp"

namespace Eden {
class QuickMenu {
public:
    QuickMenu(FileAmiibo& amiibo_, std::string folder_) : amiibo(amiibo_), folder(std::move(folder_)) {}
    ~QuickMenu() { QuickOverlay::Hide(); }
    bool Open() const { return open; }
    const AmiiboDescription& SelectedAmiibo() const { return description; }
    void Toggle() {
        open = !open;
        if (!open) { QuickOverlay::Hide(); return; }
        files = AmiiboFiles(folder);
        catalog = AmiiboCatalog::Read(folder);
        selected = std::min(selected, files.empty() ? size_t{0} : files.size() - 1);
        DescribeSelection();
        status.clear();
        cheats_page = false;
        Publish();
    }
    void Press(unsigned buttons) {
        using namespace ps5::pad;
        if (!open) return;
        if (buttons & kButtonCircle) { Toggle(); return; }
        if (!buttons) return;
        if (buttons & (kButtonUp | kButtonDown)) {
            cheats_page = !cheats_page; status.clear(); Publish(); return;
        }
        if (cheats_page) {
            const auto view = LiveCheats::Read();
            if (!view.rows.empty()) {
                cheat_selected = std::min(cheat_selected, view.rows.size() - 1);
                if (buttons & kButtonLeft) cheat_selected = (cheat_selected + view.rows.size() - 1) % view.rows.size();
                if (buttons & kButtonRight) cheat_selected = (cheat_selected + 1) % view.rows.size();
                if (buttons & kButtonCross) (void)LiveCheats::Toggle(view.generation, view.rows[cheat_selected].index);
            }
            Publish(); return;
        }
        if (!files.empty() && (buttons & (kButtonLeft | kButtonRight))) {
            selected = buttons & kButtonLeft ? (selected + files.size() - 1) % files.size() : (selected + 1) % files.size();
            DescribeSelection();
            status.clear();
        }
        if (buttons & kButtonSquare) { amiibo.Remove(); status = "Tag removed"; }
        else if (buttons & kButtonCross) {
            if (files.empty()) status = "Copy .bin files to amiibo/";
            else if (amiibo.State() == FileAmiibo::Driver::State::TagNearby) status = "Remove the current tag first";
            else switch (amiibo.Load(files[selected])) {
            case FileAmiibo::Driver::Info::Success: status = "Tag presented"; break;
            case FileAmiibo::Driver::Info::WrongDeviceState: status = "Ask the game to scan first"; break;
            case FileAmiibo::Driver::Info::NotAnAmiibo: status = "Invalid .bin file"; break;
            default: status = "Could not read .bin file"; break;
            }
        }
        Publish();
    }
private:
    void DescribeSelection() {
        description = files.empty() ? AmiiboDescription{} : DescribeAmiibo(files[selected], folder, catalog);
        picture = LoadQuickPicture(description.image_path);
    }
    void Publish() const {
        if (cheats_page) {
            const auto view = LiveCheats::Read();
            std::string text = "CHEATS ";
            if (view.rows.empty()) text += "No cheats loaded";
            else {
                const auto index = std::min(cheat_selected, view.rows.size() - 1);
                const auto& row = view.rows[index];
                text += std::to_string(index + 1) + "/" + std::to_string(view.rows.size()) +
                    (row.enabled ? " ON " : " OFF ") + row.name.substr(0, 24);
            }
            QuickOverlay::Show(text + " | L/R | CROSS Toggle | CIRCLE Close | UP/DOWN Amiibo | This session");
            return;
        }
        std::string text = "AMIIBO ";
        if (!status.empty()) text += status;
        else if (files.empty()) text += "No .bin files in amiibo/";
        else text += std::to_string(selected + 1) + "/" + std::to_string(files.size()) + " " +
            description.label.substr(0, 24);
        text += " | L/R | CROSS Scan | SQUARE Remove | CIRCLE Close | UP/DOWN Cheats";
        QuickOverlay::Show(text, picture);
    }
    FileAmiibo& amiibo;
    std::string folder, status;
    std::vector<std::string> files;
    AmiiboCatalog catalog;
    AmiiboDescription description;
    std::shared_ptr<const QuickPicture> picture;
    size_t selected = 0;
    size_t cheat_selected = 0;
    bool cheats_page = false;
    bool open = false;
};
} // namespace Eden
