// SPDX-License-Identifier: GPL-3.0-or-later
#include "vfs.h"
#include <cctype>

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
        if (!AllowAllocation(size)) return 0;
        return held.session->Read(held.entry, data, size, offset);
    }
    std::vector<u8> ReadBytes(std::size_t size, std::size_t offset) const override {
        if (!AllowAllocation(size)) return {};
        return FileSys::VfsFile::ReadBytes(size, offset);
    }
    std::vector<u8> ReadAllBytes() const override {
        if (!AllowAllocation(GetSize())) return {};
        return ReadBytes(GetSize(), 0);
    }
    std::size_t Write(const u8*, std::size_t, std::size_t) override { return 0; }
    bool Rename(std::string_view) override { return false; }
private:
    Opened held; // shared owner, independent of any RealVfsFilesystem and the launcher
    std::string uri;
};
class Directory final : public FileSys::ReadOnlyVfsDirectory {
public:
    explicit Directory(OpenedDirectory held) : held(std::move(held)) {}
    std::string GetName() const override { return held.path.substr(held.path.find_last_of('/') + 1); }
    std::string GetFullPath() const override { return Uri(held.id, held.path); }
    FileSys::VirtualDir GetParentDirectory() const override { return nullptr; }
    bool IsReadable() const override { return held.session->Healthy(); }
    std::vector<FileSys::VirtualFile> GetFiles() const override {
        std::vector<Entry> entries;
        if (!held.session->List(held.path, &entries)) return {};
        std::vector<FileSys::VirtualFile> files;
        for (auto& entry : entries) {
            const auto name = entry.path;
            if (!entry.directory && name.size() >= 4) {
                auto extension = name.substr(name.size() - 4);
                for (auto& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (extension == ".nsp" || extension == ".xci")
                    files.push_back(std::make_shared<File>(Opened{held.session, std::move(entry)}, Uri(held.id, name)));
            }
        }
        return files;
    }
    std::vector<FileSys::VirtualDir> GetSubdirectories() const override {
        if (held.depth >= 1) return {}; // fixed root + one child level, regardless of NAS depth
        std::vector<Entry> entries;
        if (!held.session->List(held.path, &entries)) return {};
        std::vector<FileSys::VirtualDir> directories;
        for (const auto& e : entries) if (e.directory)
            directories.push_back(std::make_shared<Directory>(OpenedDirectory{held.session, held.id, e.path, held.depth + 1}));
        return directories;
    }
private:
    OpenedDirectory held; // provider/offset-NCA consumers retain this exact session, never manager lookups
};
} // namespace
FileSys::VirtualFile OpenVfs(std::string_view uri) {
    auto opened = Open(uri);
    if (!opened.session) return nullptr;
    return std::make_shared<File>(std::move(opened), std::string{uri});
}
FileSys::VirtualDir OpenVfsDirectory(std::string_view uri) {
    auto opened = OpenDirectory(uri);
    if (!opened.session) return {};
    Entry entry;
    if (!opened.session->Stat(opened.path, &entry) || !entry.directory) return {};
    return std::make_shared<Directory>(std::move(opened));
}
} // namespace Eden::Network
