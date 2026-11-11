// ProsperoEden - Launcher: Storage root browser, Language and About.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pe/ui/launcher.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace pe::ui
{

using audio::Cue;

namespace
{

constexpr Rect kListPanel{108.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kDetailPanel{980.0f, 188.0f, 820.0f, 720.0f};
constexpr Rect kWindow{138.0f, 292.0f, 760.0f, 518.0f};
constexpr float kRowHeight = 78.0f;

std::string join_path(const std::string &directory, const std::string &name)
{
    return directory == "/" ? "/" + name : directory + "/" + name;
}

std::string parent_path(const std::string &directory)
{
    const std::size_t slash = directory.find_last_of('/');
    return slash == 0 || slash == std::string::npos ? "/" : directory.substr(0, slash);
}

std::string short_path(const std::string &path, std::size_t limit)
{
    return path.size() <= limit ? path : "..." + path.substr(path.size() - (limit - 3));
}

// "3 games": the count goes where the text says {0}; one and many are separate texts, so a
// language can word each as it needs.
std::string counted(int count, const char *one, const char *many)
{
    return fill(count == 1 ? one : many, {std::to_string(count)});
}

// A folder mark from two rounded shapes; `up` adds the arrow of the parent folder.
void draw_folder(Canvas &c, float x, float cy, Color ink, bool up)
{
    c.list.rounded_rect({x, cy - 13.0f, 18.0f, 8.0f}, 3.0f, ink);
    c.list.rounded_rect({x, cy - 9.0f, 40.0f, 24.0f}, 4.0f, ink);
    if (up)
    {
        const Color dark = theme::kPanel;
        c.list.line(x + 20.0f, cy + 9.0f, x + 20.0f, cy - 3.0f, 2.4f, dark);
        c.list.line(x + 14.0f, cy + 2.0f, x + 20.0f, cy - 4.0f, 2.4f, dark);
        c.list.line(x + 26.0f, cy + 2.0f, x + 20.0f, cy - 4.0f, 2.4f, dark);
    }
}

} // namespace

// ---------------------------------------------------------------- storage

void Launcher::enter_files()
{
    // Start where the player's files are (or will be after reopening), else the nearest
    // folder that can be opened.
    std::string start = services_.saved_location_folder(browse_location_);
    if (start.empty())
        start = services_.location_folder(browse_location_);
    files_.visible = 6;
    files_.pitch = 88.0f;
    content_directory_reenter_ = false;
    if (browse_location_ != StorageLocation::files) {
        browse_dir_ = start;
        browse_entries_.clear();
        browse_image_start_ = 0;
        files_.reset(0, 0);
        if (content_directory_.valid()) {
            content_directory_cancel_.store(true, std::memory_order_release);
            content_directory_reenter_ = true;
            say(tr("Reading content folder. Please wait or press Circle to cancel."));
            return;
        }
        (void)browse_to(start);
        return;
    }
    while (!browse_to(start) && start != "/")
        start = parent_path(start);
    message_.clear();
}

bool Launcher::browse_to(const std::string &directory)
{
    if (browse_location_ != StorageLocation::files) {
        if (content_directory_.valid()) {
            say(tr("Reading content folder. Please wait or press Circle to cancel."));
            return false;
        }
        content_directory_cancel_.store(false, std::memory_order_release);
        const auto kind = browse_location_;
        content_directory_kind_ = kind;
        try {
            content_directory_ = std::async(std::launch::async, [this, directory, kind] {
                ContentDirectory result;
                result.directory = directory; result.kind = kind;
                if (content_directory_cancel_.load(std::memory_order_acquire)) return result;
                result.ok = services_.content_entries(directory, &result.folders,
                    kind == StorageLocation::wallpapers ? &result.images : nullptr, &content_directory_cancel_);
                return result;
            });
            say(tr("Reading content folder. Please wait or press Circle to cancel."));
            return true;
        } catch (...) {
            say(tr("This folder cannot be opened."), true);
            return false;
        }
    }
    std::vector<std::string> folders;
    if (!services_.folders(directory, &folders))
        return false;
    browse_dir_ = directory;
    browse_entries_.clear();
    const bool fixed_root = services_.filesystem_access() == -2 &&
                            directory == services_.files_folder();
    if (directory != "/" && !fixed_root)
        browse_entries_.push_back("..");
    browse_entries_.insert(browse_entries_.end(), folders.begin(), folders.end());
    browse_image_start_ = static_cast<int>(browse_entries_.size());
    if (browse_location_ == StorageLocation::wallpapers) {
        const auto images = services_.wallpaper_files(directory);
        browse_entries_.insert(browse_entries_.end(), images.begin(), images.end());
    }
    files_.reset(static_cast<int>(browse_entries_.size()), 0);
    // The folder shown is what TRIANGLE saves: show what it holds.
    folder_info_ = browse_location_ == StorageLocation::files ? services_.folder_info(directory) : FolderInfo{};
    return true;
}

void Launcher::finish_content_work()
{
    if (content_directory_.valid() && content_directory_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        const bool current = !content_directory_cancel_.load(std::memory_order_acquire) &&
            !content_directory_reenter_ && screen_ == Screen::files && content_directory_kind_ == browse_location_;
        try {
            auto result = content_directory_.get();
            if (current && result.kind == browse_location_) {
                if (!result.ok) {
                    if (browse_entries_.empty() && browse_dir_ != "/") {
                        browse_entries_.push_back("..");
                        browse_image_start_ = 1;
                        files_.reset(1, 0);
                    }
                    say(tr("This folder cannot be opened."), true);
                } else {
                    browse_dir_ = std::move(result.directory);
                    browse_entries_.clear();
                    if (browse_dir_ != "/") browse_entries_.push_back("..");
                    browse_entries_.insert(browse_entries_.end(), result.folders.begin(), result.folders.end());
                    browse_image_start_ = static_cast<int>(browse_entries_.size());
                    browse_entries_.insert(browse_entries_.end(), result.images.begin(), result.images.end());
                    files_.reset(static_cast<int>(browse_entries_.size()), 0);
                    message_.clear();
                }
            }
        } catch (...) {
            if (current) say(tr("This folder cannot be opened."), true);
        }
        if (content_directory_reenter_) {
            content_directory_reenter_ = false;
            if (screen_ == Screen::files && browse_location_ != StorageLocation::files)
                (void)browse_to(browse_dir_); // old owner drained; start only the current screen's request
        }
    }
    if (content_write_.valid() && content_write_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        bool saved = false;
        try { saved = content_write_.get(); } catch (...) {}
        // A successful commit wins over a late cancellation request. Never
        // report "unchanged" after the settings publication already completed.
        say(saved ? content_write_wallpaper_ ? tr("Wallpaper saved. Reopen Encore to use it.") :
                                               tr("Storage root saved. Reopen Encore to use it.") :
            content_write_cancel_.load(std::memory_order_acquire) ? tr("Content change cancelled before saving.") :
                                               tr("Could not save the storage root. Please try again."), !saved);
        cue(saved ? Cue::saved : Cue::error);
    }
}

void Launcher::save_content_location(std::string directory, std::string image)
{
    if (content_write_.valid() || content_directory_.valid()) {
        say(tr("Reading content folder. Please wait or press Circle to cancel.")); return;
    }
    content_write_cancel_.store(false, std::memory_order_release);
    content_write_wallpaper_ = !image.empty();
    const auto kind = browse_location_;
    try {
        content_write_ = std::async(std::launch::async,
            [this, kind, directory = std::move(directory), image = std::move(image)] {
                return image.empty() ? services_.set_location_folder(kind, directory, &content_write_cancel_) :
                                       services_.set_wallpaper(directory, image, &content_write_cancel_);
            });
        say(tr("Saving content location. Please wait or press Circle to cancel."));
    } catch (...) { say(tr("Could not save the storage root. Please try again."), true); }
}

void Launcher::press_files(Key key)
{
    if (content_directory_.valid()) {
        if (key == Key::circle) {
            content_directory_cancel_.store(true, std::memory_order_release);
            content_directory_reenter_ = false;
            open(Screen::settings, false);
        } else say(tr("Reading content folder. Please wait or press Circle to cancel."));
        return;
    }
    const int count = static_cast<int>(browse_entries_.size());
    switch (key)
    {
    case Key::circle:
        open(Screen::settings, false);
        return;
    case Key::up:
    case Key::down:
        if (files_.move(key == Key::down ? 1 : -1))
            cue(Cue::focus);
        return;
    case Key::l1:
    case Key::r1:
        if (files_.page(key == Key::r1 ? 1 : -1))
            cue(Cue::page);
        return;
    case Key::cross:
    {
        if (count == 0)
            return;
        const std::string entry = browse_entries_[static_cast<std::size_t>(files_.selected)];
        if (browse_location_ == StorageLocation::wallpapers && files_.selected >= browse_image_start_) {
            save_content_location(browse_dir_, entry);
            return;
        }
        const std::string from = browse_dir_;
        const bool up = entry == "..";
        if (!browse_to(up ? parent_path(browse_dir_) : join_path(browse_dir_, entry)))
        {
            say(tr("This folder cannot be opened."), true);
            cue(Cue::error);
            return;
        }
        message_.clear();
        // Going up keeps the folder just left in view.
        if (up && browse_location_ == StorageLocation::files)
        {
            const std::string left = from.substr(from.find_last_of('/') + 1);
            for (int i = 0; i < static_cast<int>(browse_entries_.size()); ++i)
                if (browse_entries_[static_cast<std::size_t>(i)] == left)
                {
                    files_.reset(files_.count, i);
                    break;
                }
        }
        cue(Cue::select);
        return;
    }
    case Key::triangle:
    {
        if (browse_location_ != StorageLocation::files) { save_content_location(browse_dir_); return; }
        const bool saved = services_.set_location_folder(browse_location_, browse_dir_);
        say(saved ? tr("Storage root saved. Reopen Encore to use it.") :
                    tr("Could not save the storage root. Please try again."),
            !saved);
        cue(saved ? Cue::saved : Cue::error);
        return;
    }
    case Key::square:
    {
        if (browse_location_ != StorageLocation::files) { save_content_location({}); return; }
        const std::string fallback = services_.default_location_folder(browse_location_);
        const bool saved = services_.set_location_folder(browse_location_,
            browse_location_ == StorageLocation::files ? fallback : std::string{});
        (void)browse_to(fallback);
        say(saved ? tr("Default location saved. Reopen Encore to use it.") :
                    tr("Could not save the storage root. Please try again."),
            !saved);
        cue(saved ? Cue::saved : Cue::error);
        return;
    }
    default:
        return;
    }
}

void Launcher::draw_files(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const int count = static_cast<int>(browse_entries_.size());
    draw_top_nav(c, 3);
    text_shrink(c, browse_location_ == StorageLocation::games ? tr("Games folder") :
                   browse_location_ == StorageLocation::wallpapers ? tr("Wallpaper folder") : tr("Storage"),
                108.0f, baseline(138.0f, 34.0f, theme::kHeading),
                theme::kHeading, theme::kTitle, 880.0f);
    text_shrink(c, browse_location_ == StorageLocation::files ?
                tr("Choose a root for keys, firmware, updates and mods") :
                tr("Independent location. No files or saves are moved. Applies after restart."), 108.0f,
                baseline(166.0f, 22.0f, 18.0f), 18.0f, theme::kMeta, 1180.0f);

    // ---- folders ----
    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("FOLDERS"), 138.0f, baseline(208.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text(c, short_path(browse_dir_, 52), 138.0f, baseline(246.0f, 28.0f, theme::kSmall),
         theme::kSmall, theme::kBody);
    if (count == 0)
    {
        text(c, tr("No folders here."), kListPanel.x + kListPanel.w * 0.5f,
             baseline(488.0f, 38.0f, theme::kText24), theme::kText24, theme::kCopy, Align::center);
    }
    else
    {
        list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
        const auto row_rect = [&](int row) -> Rect
        {
            return {kWindow.x, kWindow.y + static_cast<float>(row) * files_.pitch - files_.scroll(),
                    kWindow.w, kRowHeight};
        };
        for (int row = files_.first_row(); row <= files_.last_row(); ++row)
        {
            list.push_opacity(files_.row_alpha(row, kRowHeight));
            plate_rest(c, kListPlate, row_rect(row));
            list.pop_opacity();
        }
        plate_focus(c, kListPlate,
                    {kWindow.x, kWindow.y + files_.cursor() - files_.scroll(), kWindow.w, kRowHeight},
                    1.0f);
        for (int row = files_.first_row(); row <= files_.last_row(); ++row)
        {
            const std::string &entry = browse_entries_[static_cast<std::size_t>(row)];
            const bool up = entry == "..";
            const bool image = browse_location_ == StorageLocation::wallpapers && row >= browse_image_start_;
            const Rect r = row_rect(row);
            list.push_opacity(files_.row_alpha(row, kRowHeight));
            if (!image) draw_folder(c, r.x + 24.0f, r.y + kRowHeight * 0.5f,
                                    up ? theme::kLimePale : theme::kCopy, up);
            else c.list.bordered_rect({r.x + 24.0f, r.y + 22.0f, 40.0f, 30.0f}, 3.0f,
                                      theme::kRow, 2.0f, theme::kLimePale);
            const float tag = text(c, image ? tr("Select") : up ? tr("UP") : tr("OPEN"), r.x + 730.0f,
                                   baseline(r.y, kRowHeight, theme::kSmall), theme::kSmall,
                                   theme::kMeta, Align::right);
            text_fit(c, up ? tr("Parent folder") : entry, r.x + 84.0f,
                     baseline(r.y, kRowHeight, theme::kText24), theme::kText24,
                     up ? theme::kLimePale : theme::kText, 730.0f - 84.0f - tag - 24.0f);
            list.pop_opacity();
        }
        list.pop_clip();
        scrollbar(c, files_, 910.0f, kWindow.y, kWindow.h);
    }
    text(c, list_position(count > 0 ? files_.selected + 1 : 0, count), 898.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    // ---- what the folder shown holds ----
    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    if (browse_location_ != StorageLocation::files) {
        text_block(c, browse_dir_, 1016.0f, baseline(230.0f, 40.0f, 28.0f),
                   28.0f, 40.0f, theme::kTitle, 748.0f, 3);
        const bool image = browse_location_ == StorageLocation::wallpapers &&
                           files_.selected >= browse_image_start_ && files_.selected < count;
        if (image) {
            const std::string path = "wallpaper:" + join_path(browse_dir_, browse_entries_[std::size_t(files_.selected)]);
            cover_crop(c, path, {1016.0f, 384.0f, 748.0f, 320.0f}, 10.0f);
        } else {
            text_block(c, browse_location_ == StorageLocation::games ?
                tr("Choose the folder that directly contains your games. Keys, firmware, mods and saves keep their existing locations.") :
                tr("Open a folder and select a PNG, JPEG or TGA image. Missing or invalid artwork uses the packaged backdrop."),
                1016.0f, baseline(388.0f, 40.0f, 25.0f), 25.0f, 38.0f, theme::kCopy, 748.0f, 6);
        }
        notice_block(c, message_.empty() ? tr("No files are copied. Reopen Encore after saving.") : message_,
                     1016.0f, baseline(742.0f, 36.0f, 24.0f), 24.0f, 36.0f,
                     message_warning_ ? theme::kWarning : theme::kCopy, 748.0f, 3, message_warning_);
        static constexpr Hint hints[] = {{Pad::cross, TR("Open / select")}, {Pad::circle, TR("Back")},
            {Pad::triangle, TR("Use this folder")}, {Pad::square, TR("Default location")}};
        draw_footer(c, hints, 4);
        return;
    }
    text(c, tr("THIS ROOT"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_block(c, browse_dir_, 1016.0f, baseline(250.0f, 42.0f, theme::kHeading), theme::kHeading,
               42.0f, theme::kTitle, 748.0f, 2);
    const float column = value_column(
        c, {tr("KEYS"), tr("FIRMWARE"), tr("GAMES"), tr("IN USE"), tr("ACCESS")}, 1016.0f, 1216.0f,
        theme::kSmall, 2.0f);
    // A line whose value is in the warning colour carries the warning mark too.
    const auto line = [&](float top, const char *label, const std::string &value, Color color)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kLabel,
             Align::left, 2.0f);
        notice(c, value, column, baseline(top, 30.0f, theme::kSmall), theme::kSmall, color,
               1764.0f - column, color.r == theme::kWarning.r && color.g == theme::kWarning.g);
    };
    const auto state = [](bool ready) { return ready ? theme::kLimePale : theme::kWarning; };
    line(378.0f, tr("KEYS"), folder_info_.keys ? tr("prod.keys found") : tr("prod.keys missing"),
         state(folder_info_.keys));
    line(434.0f, tr("FIRMWARE"),
         folder_info_.firmware < 0 ? tr("No firmware folder") :
                                     counted(folder_info_.firmware, tr("{0} NCA file"), tr("{0} NCA files")),
         state(folder_info_.firmware > 0));
    line(490.0f, tr("GAMES"),
         folder_info_.games < 0 ? tr("No roms folder") : counted(folder_info_.games, tr("{0} game"), tr("{0} games")),
         state(folder_info_.games > 0));
    list.rounded_rect({1016.0f, 554.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    const std::string in_use_folder = services_.files_folder();
    const std::string saved = services_.saved_files_folder();
    line(578.0f, tr("IN USE"),
         !saved.empty() && saved != in_use_folder ? fill(tr("Next launch: {0}"), {short_path(saved, 32)}) :
                                                    short_path(in_use_folder, 40),
         theme::kValue);
    const int access = services_.filesystem_access();
    line(634.0f, tr("ACCESS"),
         access == 0 ? tr("Full filesystem") :
         access == -2 ? tr("Sandbox only") :
                        fill(tr("Sandboxed (code {0}): app folder only"), {std::to_string(access)}),
         state(access == 0 || access == -2));
    list.rounded_rect({1016.0f, 698.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    notice_block(c,
                 message_.empty() ?
                     tr("Encore uses the same keys, firmware, roms, updates and mods folders under every storage root.") :
                     message_,
                 1016.0f, baseline(722.0f, 36.0f, theme::kText24), theme::kText24, 36.0f,
                 message_.empty() ? theme::kCopy :
                 message_warning_ ? theme::kWarning : theme::kLimePale,
                 748.0f, 3, !message_.empty() && message_warning_);

    static constexpr Hint kHints[] = {{Pad::cross, TR("Open")},
                                      {Pad::circle, TR("Back")},
                                      {Pad::triangle, TR("Use this root")},
                                      {Pad::square, TR("Internal storage")},
                                      {Pad::updown, TR("Browse")},
                                      {Pad::l1, TR("Page"), Pad::r1}};
    draw_footer(c, kHints, 6);
}

// ---------------------------------------------------------------- language

void Launcher::enter_language()
{
    language_.visible = 6;
    language_.pitch = 88.0f;
    language_.reset(static_cast<int>(services_.language_labels().size()), prefs_.language);
    message_.clear();
}

void Launcher::press_language(Key key)
{
    switch (key)
    {
    case Key::circle:
        open(Screen::settings, false);
        // The home screen names the language the last game will use, and so does the Library.
        read_home();
        name_home_games();
        finish_scan(false);
        start_scan();
        return;
    case Key::up:
    case Key::down:
        if (language_.move(key == Key::down ? 1 : -1))
        {
            message_.clear();
            cue(Cue::focus);
        }
        return;
    case Key::l1:
    case Key::r1:
        if (language_.page(key == Key::r1 ? 1 : -1))
        {
            message_.clear();
            cue(Cue::page);
        }
        return;
    case Key::cross:
    {
        const int before = prefs_.language;
        prefs_.language = language_.selected;
        const bool saved = services_.set_preferences(prefs_);
        if (!saved)
        {
            prefs_.language = before;
            say(tr("Could not save. Please try again."), true);
            cue(Cue::error);
            return;
        }
        // Recreate the launcher immediately: the new catalog, script-specific system fonts and
        // translated label caches must all change together. Games also use the same preference.
        restart_requested_ = true;
        cue(Cue::saved);
        return;
    }
    default:
        return;
    }
}

void Launcher::draw_language(Canvas &c)
{
    gfx::DrawList &list = c.list;
    const auto &labels = services_.language_labels();
    const int count = static_cast<int>(labels.size());
    draw_top_nav(c, 3);
    text_shrink(c, tr("Language"), 108.0f, baseline(138.0f, 34.0f, theme::kHeading),
                theme::kHeading, theme::kTitle, 880.0f);
    text_shrink(c, tr("Choose the language games use"), 108.0f,
                baseline(166.0f, 22.0f, 18.0f), 18.0f, theme::kMeta, 1180.0f);

    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("LANGUAGES"), 138.0f, baseline(208.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
    const auto row_rect = [&](int row) -> Rect
    {
        return {kWindow.x, kWindow.y + static_cast<float>(row) * language_.pitch - language_.scroll(),
                kWindow.w, kRowHeight};
    };
    for (int row = language_.first_row(); row <= language_.last_row(); ++row)
    {
        list.push_opacity(language_.row_alpha(row, kRowHeight));
        plate_rest(c, kListPlate, row_rect(row));
        list.pop_opacity();
    }
    plate_focus(c, kListPlate,
                {kWindow.x, kWindow.y + language_.cursor() - language_.scroll(), kWindow.w,
                 kRowHeight},
                1.0f);
    for (int row = language_.first_row(); row <= language_.last_row(); ++row)
    {
        const Rect r = row_rect(row);
        list.push_opacity(language_.row_alpha(row, kRowHeight));
        // The language in use carries a lime tag; its name has the room the tag leaves.
        const float width = text_width(c, tr("IN USE"), 18.0f, 2.0f) + 28.0f;
        text_shrink(c, labels[static_cast<std::size_t>(row)], r.x + 26.0f,
                    baseline(r.y, kRowHeight, theme::kText24), theme::kText24, theme::kText,
                    row == prefs_.language ? 734.0f - 26.0f - width - 24.0f : 708.0f);
        if (row == prefs_.language)
        {
            // The tag is as tall as its letters need.
            const float tall = std::max(30.0f, text_size(18.0f) + 10.0f);
            const Rect tag{r.x + 734.0f - width, r.y + (kRowHeight - tall) * 0.5f, width, tall};
            list.bordered_rect(tag, tall * 0.5f, theme::kLime.with_alpha(0.16f), 1.0f,
                               theme::kLime.with_alpha(0.55f));
            text(c, tr("IN USE"), tag.x + tag.w * 0.5f, baseline(tag.y, tall, 18.0f), 18.0f,
                 theme::kLimePale, Align::center, 2.0f);
        }
        list.pop_opacity();
    }
    list.pop_clip();
    scrollbar(c, language_, 910.0f, kWindow.y, kWindow.h);
    text(c, list_position(language_.selected + 1, count), 898.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("SELECTED"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    const auto label_at = [&](int index) -> std::string
    {
        return index >= 0 && index < count ? labels[static_cast<std::size_t>(index)] : std::string{};
    };
    text_shrink(c, label_at(language_.selected), 1016.0f, baseline(250.0f, 42.0f, theme::kHeading),
                theme::kHeading, theme::kTitle, 748.0f);
    const float column =
        value_column(c, {tr("REGION"), tr("IN USE")}, 1016.0f, 1216.0f, theme::kSmall, 2.0f);
    const auto line = [&](float top, const char *label, const std::string &value, Color color)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kLabel,
             Align::left, 2.0f);
        text_shrink(c, value, column, baseline(top, 30.0f, theme::kSmall), theme::kSmall, color,
                    1764.0f - column);
    };
    line(378.0f, tr("REGION"), services_.language_region(language_.selected), theme::kLimePale);
    line(434.0f, tr("IN USE"), label_at(prefs_.language), theme::kValue);
    list.rounded_rect({1016.0f, 554.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text_block(c,
               tr("Games use this language when they offer it, and their own default otherwise. It "
               "applies when a game starts."),
               1016.0f, baseline(578.0f, 34.0f, 22.0f), 22.0f, 34.0f, theme::kCopy, 748.0f, 3,
               kShrink);
    if (!message_.empty())
        notice_block(c, message_, 1016.0f, baseline(722.0f, 34.0f, 22.0f), 22.0f, 34.0f,
                     message_warning_ ? theme::kWarning : theme::kLimePale, 748.0f, 2,
                     message_warning_);

    static constexpr Hint kHints[] = {{Pad::cross, TR("Choose")},
                                      {Pad::circle, TR("Back")},
                                      {Pad::updown, TR("Browse")},
                                      {Pad::l1, TR("Page"), Pad::r1}};
    draw_footer(c, kHints, 4);
}

// ---------------------------------------------------------------- about

void Launcher::draw_about(Canvas &c)
{
    gfx::DrawList &list = c.list;
    draw_top_nav(c, 3);
    text_shrink(c, tr("About Eden Encore"), 108.0f, baseline(138.0f, 34.0f, theme::kHeading),
                theme::kHeading, theme::kTitle, 880.0f);
    text_shrink(c, tr("Credits and setup"), 108.0f,
                baseline(166.0f, 22.0f, 18.0f), 18.0f, theme::kMeta, 1180.0f);

    glass(c, kListPanel, 26.0f, theme::kPanel.with_alpha(0.80f), theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("PROJECT CREDITS"), 144.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_shrink(c, tr("Powered by Eden"), 144.0f, baseline(250.0f, 54.0f, theme::kLead),
                theme::kLead, theme::kTitle, 748.0f);
    text_block(c, tr("All credit for the Eden emulator goes to its developers and contributors."),
               144.0f, baseline(310.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 2, kShrink);
    text(c, "eden-emu.dev", 144.0f, baseline(384.0f, 36.0f, theme::kText24), theme::kText24,
         theme::kLimePale);
    list.rounded_rect({144.0f, 444.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text(c, tr("THANKS"), 144.0f, baseline(464.0f, 30.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 2.0f);
    text_block(c,
               tr("Thanks to the whole PS5 homebrew community and to every developer whose drivers, "
               "tools and libraries make Eden Encore possible."),
               144.0f, baseline(500.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 3, kShrink);
    list.rounded_rect({144.0f, 628.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text(c, tr("PS5 13.60 FORK"), 144.0f, baseline(648.0f, 30.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 2.0f);
    text_block(c, tr("Eden Encore builds on the proven ProsperoEden 0.40 PS5 base for firmware 13.60, with the Eden core, safer recovery, and targeted compatibility, performance and TV-first UX improvements."),
               144.0f, baseline(684.0f, 36.0f, theme::kText24), theme::kText24, 36.0f, theme::kBody,
               748.0f, 2, kShrink);
    text_shrink(c, "GPL-3.0-or-later  /  no warranty  /  source + licenses: github.com/niakw/ProsperoEden-Encore", 144.0f,
                baseline(788.0f, 30.0f, theme::kSmall), theme::kSmall, theme::kMeta, 748.0f);
    // User-provided controller illustration, raster-resized for Home.
    // Mandatory attribution from PS5 Button Icons and Controls / CC BY 3.0.
    text_shrink(c, "DualSense icon: Zacksly (CC BY 3.0) - zacksly.itch.io", 144.0f,
                baseline(826.0f, 26.0f, theme::kSmall), theme::kSmall, theme::kMeta, 748.0f);
    text(c, version_, 892.0f, baseline(866.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::right);

    glass(c, kDetailPanel, 26.0f, theme::kPanel.with_alpha(0.80f),
          theme::kPanelEdge.with_alpha(0.55f));
    text(c, tr("GETTING STARTED"), 1016.0f, baseline(210.0f, 28.0f, theme::kSmall), theme::kSmall,
         theme::kLimePale, Align::left, 3.0f);
    text_shrink(c, tr("Supply your own files"), 1016.0f, baseline(262.0f, 54.0f, theme::kLead),
                theme::kLead, theme::kTitle, 748.0f);
    const std::string folder = services_.files_folder();
    const float column = value_column(
        c, {tr("KEYS"), tr("FIRMWARE"), tr("GAMES"), tr("UPDATES, DLC"), tr("MODS")}, 1016.0f,
        1232.0f, theme::kSmall);
    const auto line = [&](float top, const char *label, const std::string &value)
    {
        text(c, label, 1016.0f, baseline(top, 30.0f, theme::kSmall), theme::kSmall, theme::kMeta);
        text_shrink(c, value, column, baseline(top, 34.0f, theme::kText24), theme::kText24,
                    theme::kValue, 1764.0f - column);
    };
    // Five lines above the rule. A game's own mods folder is named after its ID: its settings in
    // the Library show it (and create it).
    line(384.0f, tr("KEYS"), short_path(folder + "/keys/prod.keys", 36));
    line(450.0f, tr("FIRMWARE"), short_path(folder + "/firmware/*.nca", 36));
    line(516.0f, tr("GAMES"), fill(tr("{0}/ (NSP or XCI)"),
         {short_path(services_.location_folder(StorageLocation::games), 36)}));
    line(582.0f, tr("UPDATES, DLC"), fill(tr("{0}/ (NSP or XCI)"), {short_path(folder + "/updates", 36)}));
    line(648.0f, tr("MODS"), fill(tr("{0}/ (one folder per game ID)"), {short_path(folder + "/mods", 36)}));
    list.rounded_rect({1016.0f, 712.0f, 748.0f, 1.0f}, 0.0f, theme::kRule);
    text_block(c,
               tr("Use extracted firmware NCA files. Choose the root in Settings, Storage; "
               "restart the app after changing it."),
               1016.0f, baseline(740.0f, 36.0f, theme::kText24), theme::kText24, 36.0f,
               theme::kMeta, 748.0f, 3, kShrink);

    static constexpr Hint kHints[] = {{Pad::circle, TR("Back")}};
    draw_footer(c, kHints, 1);
}

} // namespace pe::ui
