// SPDX-License-Identifier: GPL-3.0-or-later
#include "vfs.h"

namespace Eden::Network {
namespace {
// Loader identification probes the containing directory even for invalid container bytes.
// Do not expose remote siblings (deconstructed games are unsupported), but never hand that
// probe a null directory. Mutations remain rejected by the read-only base implementation.
class Container final : public FileSys::ReadOnlyVfsDirectory {
public:
    std::vector<FileSys::VirtualFile> GetFiles() const override { return {}; }
    std::vector<FileSys::VirtualDir> GetSubdirectories() const override { return {}; }
    std::string GetName() const override { return {}; }
    FileSys::VirtualDir GetParentDirectory() const override { return nullptr; }
};
class File final : public FileSys::VfsFile {
public:
    explicit File(Opened opened, std::string uri) : held(std::move(opened)), uri(std::move(uri)) {}
    std::string GetName() const override {
        const auto slash = held.entry.path.find_last_of('/');
        return held.entry.path.substr(slash == std::string::npos ? 0 : slash + 1);
    }
    std::string GetFullPath() const override { return uri; }
    std::size_t GetSize() const override { return static_cast<std::size_t>(held.entry.size); }
    bool Resize(std::size_t) override { return false; }
    FileSys::VirtualDir GetContainingDirectory() const override { return std::make_shared<Container>(); }
    bool IsWritable() const override { return false; }
    bool IsReadable() const override { return held.session->Healthy(); }
    std::size_t Read(u8* data, std::size_t size, std::size_t offset) const override {
        return held.session->Read(held.entry, data, size, offset);
    }
    std::size_t Write(const u8*, std::size_t, std::size_t) override { return 0; }
    bool Rename(std::string_view) override { return false; }
private:
    Opened held; // shared owner, independent of any RealVfsFilesystem and the launcher
    std::string uri;
};
} // namespace
FileSys::VirtualFile OpenVfs(std::string_view uri) {
    auto opened = Open(uri);
    if (!opened.session) return nullptr;
    return std::make_shared<File>(std::move(opened), std::string{uri});
}
} // namespace Eden::Network
