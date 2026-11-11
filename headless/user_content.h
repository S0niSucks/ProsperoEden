// SPDX-License-Identifier: GPL-3.0-or-later
// User-supplied wallpaper admission. No image codec, network or render work.
#pragma once
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <memory>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace Eden::UserContent {
#if !defined(PS5_NATIVE) || (defined(EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED) && EDEN_NATIVE_USER_CONTENT_IO_QUALIFIED == 1)
#define EDEN_HAS_USER_CONTENT_IO 1
inline constexpr bool kIoAvailable = true;
#else
#define EDEN_HAS_USER_CONTENT_IO 0
inline constexpr bool kIoAvailable = false;
#endif
inline constexpr std::size_t kMaxImageBytes = 16u << 20;
inline bool Cancelled(const std::atomic<bool>* cancel) {
    return cancel && cancel->load(std::memory_order_acquire);
}
inline bool ImageName(std::string_view name) {
    if (name.empty() || name.size() > 255 || name == "." || name == "..") return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':') return false;
    const auto dot = name.find_last_of('.');
    if (dot == name.npos || dot == 0) return false;
    std::string ext{name.substr(dot)};
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga";
}
struct Descriptor {
    int value = -1;
    explicit Descriptor(int fd = -1) : value(fd) {}
    ~Descriptor() { if (value >= 0) ::close(value); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
};
// Follow no links, including ancestors. Metadata checks followed by a pathname
// open alone would not pin the directory used by the subsequent image read.
inline int OpenDirectory(std::string_view path, const std::atomic<bool>* cancel = nullptr) {
#if !EDEN_HAS_USER_CONTENT_IO
    (void)path; (void)cancel;
    return -1; // no speculative new native openat/descriptor imports
#else
    if (Cancelled(cancel) || path.empty() || path.front() != '/' || path.size() > 1024 ||
        (path.size() > 1 && path.back() == '/')) return -1;
    Descriptor parent(::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
    for (std::size_t start = 1; start < path.size();) {
        if (Cancelled(cancel)) return -1;
        const auto end = path.find('/', start);
        const auto part = path.substr(start, end == path.npos ? path.npos : end - start);
        if (part.empty() || part == "." || part == "..") return -1;
        for (unsigned char c : part) if (c < 32 || c == 127 || c == '\\' || c == ':') return -1;
        const int next = ::openat(parent.value, std::string(part).c_str(),
                                  O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0) return -1;
        if (parent.value >= 0) ::close(parent.value);
        parent.value = next;
        if (end == path.npos) break;
        start = end + 1;
    }
    const int result = parent.value;
    parent.value = -1;
    return result;
#endif
}
inline bool ImageStat(const struct stat& st) {
    return S_ISREG(st.st_mode) && st.st_nlink == 1 && st.st_size >= 18 &&
           static_cast<std::uint64_t>(st.st_size) <= kMaxImageBytes;
}
// One bounded walk serves both independent browser lists. Legacy Files keeps
// its original provider; it must not be called ahead of this bounded reader.
inline bool Entries(const std::string& folder, std::vector<std::string>* folders,
                    std::vector<std::string>* images, const std::atomic<bool>* cancel = nullptr) {
    if (folders) folders->clear();
    if (images) images->clear();
#if !EDEN_HAS_USER_CONTENT_IO
    (void)folder; (void)cancel;
    return false;
#else
    try {
        Descriptor directory(OpenDirectory(folder, cancel));
        if (directory.value < 0) return false;
        const int copy = ::dup(directory.value);
        if (copy < 0) return false;
        const auto close_stream = [](DIR* stream) { (void)::closedir(stream); };
        std::unique_ptr<DIR, decltype(close_stream)> entries(::fdopendir(copy), close_stream);
        if (!entries) { ::close(copy); return false; }
        // Private results publish only after the complete successful walk. An
        // allocation exception releases both descriptors and leaves callers empty.
        std::vector<std::string> found_folders, found_images;
        for (std::size_t seen = 0; seen < 4096; ++seen) {
            if (Cancelled(cancel)) return false;
            errno = 0;
            const auto* entry = ::readdir(entries.get());
            if (!entry) { if (errno != 0) return false; break; }
            const std::string name = entry->d_name;
            if (name == "." || name == ".." || name.empty() || name.size() > 255) continue;
            if (std::any_of(name.begin(), name.end(), [](unsigned char c) {
                    return c < 32 || c == 127 || c == '/' || c == '\\' || c == ':';
                })) continue;
            struct stat st{};
            if (::fstatat(directory.value, name.c_str(), &st, AT_SYMLINK_NOFOLLOW) != 0) return false;
            if (folders && S_ISDIR(st.st_mode)) found_folders.push_back(name);
            if (images && ImageName(name) && ImageStat(st)) found_images.push_back(name);
        }
        entries.reset();
        std::sort(found_folders.begin(), found_folders.end());
        std::sort(found_images.begin(), found_images.end());
        if (found_folders.size() > 256) found_folders.resize(256);
        if (found_images.size() > 128) found_images.resize(128);
        if (Cancelled(cancel)) return false;
        if (folders) *folders = std::move(found_folders);
        if (images) *images = std::move(found_images);
        return true;
    } catch (...) {
        return false;
    }
#endif
}
inline std::vector<std::string> Images(const std::string& folder, const std::atomic<bool>* cancel = nullptr) {
    std::vector<std::string> images;
    (void)Entries(folder, nullptr, &images, cancel);
    return images;
}
inline bool ReadImage(const std::string& folder, const std::string& name,
                      std::vector<unsigned char>& bytes, const std::atomic<bool>* cancel = nullptr) {
    bytes.clear();
#if !EDEN_HAS_USER_CONTENT_IO
    (void)folder; (void)name; (void)cancel;
    return false;
#else
    if (Cancelled(cancel) || !ImageName(name)) return false;
    Descriptor directory(OpenDirectory(folder, cancel));
    if (directory.value < 0) return false;
    struct stat admitted{};
    if (::fstatat(directory.value, name.c_str(), &admitted, AT_SYMLINK_NOFOLLOW) != 0 ||
        !ImageStat(admitted)) return false;
    Descriptor file(::openat(directory.value, name.c_str(), O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC));
    struct stat before{}, after{};
    if (file.value < 0 || ::fstat(file.value, &before) != 0 || !ImageStat(before) ||
        before.st_dev != admitted.st_dev || before.st_ino != admitted.st_ino) return false;
    bytes.resize(static_cast<std::size_t>(before.st_size));
    std::size_t at = 0;
    while (at < bytes.size()) {
        if (Cancelled(cancel)) { bytes.clear(); return false; }
        const auto count = ::read(file.value, bytes.data() + at, std::min<std::size_t>(65536, bytes.size() - at));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { bytes.clear(); return false; }
        at += static_cast<std::size_t>(count);
    }
    if (::fstat(file.value, &after) != 0 || !ImageStat(after) || before.st_size != after.st_size ||
        before.st_mtime != after.st_mtime || before.st_ctime != after.st_ctime || Cancelled(cancel)) {
        bytes.clear(); return false;
    }
    return true; // private immutable bytes for the bounded codec path
#endif
}
} // namespace Eden::UserContent
#undef EDEN_HAS_USER_CONTENT_IO
