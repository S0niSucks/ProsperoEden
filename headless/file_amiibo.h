// SPDX-License-Identifier: GPL-3.0-or-later
// The port's NFC factory: Eden's actual file-backed driver, serialized across the input and
// guest service threads. No tag data is bundled, synthesized or fetched from the network.
#pragma once
#include <algorithm>
#include <dirent.h>
#include <filesystem>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include "amiibo_file.h"
#include "game_files.h"
#include "input_common/drivers/virtual_amiibo.h"
#if defined(__PROSPERO__)
#include "native_directory.h"
#endif

namespace Eden {
class FileAmiibo final : public InputCommon::InputEngine {
public:
    using Driver = InputCommon::VirtualAmiibo;
    FileAmiibo() : InputEngine("virtual_amiibo"), driver("file_amiibo") {
        PreSetController({});
        driver.PreSetController({});
        driver.SetCallback({{}, EngineInputType::Nfc, 0, {[this] { SetNfc({}, driver.GetNfc({})); }}});
    }
    Common::Input::DriverResult SetPollingMode(const PadIdentifier& id,
                                               Common::Input::PollingMode mode) override {
        const std::lock_guard lock(mutex);
        return driver.SetPollingMode(id, mode);
    }
    Common::Input::NfcState SupportsNfc(const PadIdentifier& id) const override {
        const std::lock_guard lock(mutex);
        return driver.SupportsNfc(id);
    }
    Common::Input::NfcState StartNfcPolling(const PadIdentifier& id) override {
        const std::lock_guard lock(mutex);
        return driver.StartNfcPolling(id);
    }
    Common::Input::NfcState StopNfcPolling(const PadIdentifier& id) override {
        const std::lock_guard lock(mutex);
        return driver.StopNfcPolling(id);
    }
    Common::Input::NfcState ReadAmiiboData(const PadIdentifier& id, std::vector<u8>& data) override {
        const std::lock_guard lock(mutex);
        return driver.ReadAmiiboData(id, data);
    }
    Common::Input::NfcState WriteNfcData(const PadIdentifier& id, const std::vector<u8>& data) override {
        const std::lock_guard lock(mutex);
        if (driver.GetCurrentState() != Driver::State::TagNearby || data.size() != 0x21c)
            return Common::Input::NfcState::WriteFailed;
        return driver.WriteNfcData(id, data);
    }
    Driver::State State() const {
        const std::lock_guard lock(mutex);
        return driver.GetCurrentState();
    }
    Driver::Info Load(const std::string& path) {
        const std::lock_guard lock(mutex);
        if (driver.GetCurrentState() != Driver::State::WaitingForAmiibo)
            return Driver::Info::WrongDeviceState;
        if (!ValidFile(path)) return Driver::Info::NotAnAmiibo;
        // The filename overload bounds its read and normalizes signed dumps before calling
        // Eden's span overload. Never expose that span overload to callers (572 != 540 bytes).
        return driver.LoadAmiibo(path);
    }
    void Remove() {
        const std::lock_guard lock(mutex);
        (void)driver.CloseAmiibo();
    }
    static bool ValidFile(const std::string& path) {
        // Native lstat can be denied even for a readable app file. Opening without following
        // links gives fstat a real descriptor and does not accept a symlink as a tag file.
        return AmiiboFile(path).IsOpen();
    }
private:
    // Notifications may synchronously query the output factory again on this same thread.
    mutable std::recursive_mutex mutex;
    Driver driver;
};

inline std::vector<std::filesystem::path> AmiiboFolderEntries(const std::string& folder, size_t limit) {
    const int fd = OpenAmiiboPath(folder, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return {};
    struct Close { int fd; ~Close() { if (fd >= 0) close(fd); } } close_fd{fd};
    std::vector<std::filesystem::path> paths;
    std::error_code error;
#if defined(__PROSPERO__)
    const auto entries = ReadNativeDirectoryFd(fd, folder, error, limit);
    if (error) return {};
    for (const auto& entry : entries) paths.push_back(entry.path());
#else
    DIR* directory = fdopendir(fd);
    if (!directory) return {};
    close_fd.fd = -1; // fdopendir/closedir now own this verified descriptor
    struct CloseDirectory { DIR* value; ~CloseDirectory() { closedir(value); } } close_directory{directory};
    while (paths.size() < limit) {
        errno = 0;
        const auto* entry = readdir(directory);
        if (!entry) { if (errno) error = {errno, std::generic_category()}; break; }
        const std::string_view name{entry->d_name};
        if (name != "." && name != "..") paths.push_back(std::filesystem::path(folder) / name);
    }
#endif
    if (error) return {};
    std::sort(paths.begin(), paths.end());
    return paths;
}

inline std::vector<std::string> AmiiboFiles(const std::string& folder) {
    std::vector<std::string> files;
    size_t remaining = 4096; // one budget for the whole tree, not one per subfolder
    const auto scan = [&](const auto& self, const std::string& directory, unsigned depth) -> void {
        const auto entries = AmiiboFolderEntries(directory, remaining);
        remaining -= entries.size();
        for (const auto& path : entries) {
            const auto name = path.filename().string();
            if (name.empty() || name[0] == '.' || name.size() > 255 || path.string().size() > 4095) continue;
            if (EndsWithNoCase(name, ".bin") && FileAmiibo::ValidFile(path.string())) {
                files.push_back(path.string());
            } else if (depth < 8 && remaining) {
                // A failed no-follow directory open also rejects regular files, FIFOs and links.
                self(self, path.string(), depth + 1);
            }
        }
    };
    if (!folder.empty()) scan(scan, folder, 0);
    std::sort(files.begin(), files.end());
    return files;
}
} // namespace Eden
