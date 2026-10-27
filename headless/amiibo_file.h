// SPDX-License-Identifier: GPL-3.0-or-later
// Used only by the derived NFC driver: validate and access the same open file, including writes.
#pragma once
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <fcntl.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace Eden {
// O_NOFOLLOW only checks the last component of open(). Walk from directory descriptors so
// a collection subfolder cannot redirect reads or guest writes through an ancestor symlink.
// The caller owns the returned descriptor. This never creates files or accepts '..'.
inline int OpenAmiiboPath(const std::string& path, int flags) {
    if (path.empty() || path.size() > 4095 || path.find('\0') != std::string::npos) return -1;
    int parent = open(path.front() == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (parent < 0) return -1;
    std::string_view remaining{path};
    unsigned depth = 0;
    while (!remaining.empty()) {
        if (remaining.front() == '/') { remaining.remove_prefix(1); continue; }
        const auto slash = remaining.find('/');
        const auto part = remaining.substr(0, slash);
        if (part == "." || part == ".." || part.size() > 255 || ++depth > 64) break;
        const bool last = slash == std::string_view::npos;
        const auto component = std::string(part);
        const int next = openat(parent, component.c_str(),
            (last ? flags : O_RDONLY | O_DIRECTORY) | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        const int error = errno;
        close(parent);
        if (last || next < 0) { errno = error; return next; }
        parent = next;
        remaining.remove_prefix(slash + 1);
    }
    close(parent);
    return -1;
}

class AmiiboFile final {
public:
    explicit AmiiboFile(const std::string& path, bool writable = false) : writable(writable) {
        fd = OpenAmiiboPath(path, writable ? O_RDWR : O_RDONLY);
        struct stat info {};
        if (fd < 0) return;
        if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) ||
            (info.st_size != 0x214 && info.st_size != 0x21c && info.st_size != 0x23c)) {
            close(fd); fd = -1; return;
        }
        size = static_cast<std::size_t>(info.st_size);
    }
    ~AmiiboFile() { if (fd >= 0) close(fd); }
    AmiiboFile(const AmiiboFile&) = delete;
    AmiiboFile& operator=(const AmiiboFile&) = delete;
    bool IsOpen() const { return fd >= 0; }
    std::size_t GetSize() const { return size; }
    std::size_t Read(std::span<unsigned char> data) const {
        if (fd < 0 || data.size() > 0x21c) return 0;
        const auto count = std::min(size, data.size());
        std::size_t done = 0;
        while (done < count) {
            const auto n = pread(fd, data.data() + done, count - done, done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) break;
            done += static_cast<std::size_t>(n);
        }
        return done;
    }
    bool Write(std::span<const unsigned char> data) const {
        if (fd < 0 || !writable || data.size() != 0x21c) return false;
        std::size_t done = 0;
        while (done < data.size()) {
            const auto n = pwrite(fd, data.data() + done, data.size() - done, done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) return false;
            done += static_cast<std::size_t>(n);
        }
        return true; // preserve an optional signature; a 532-byte dump gains its password bytes
    }
private:
    int fd = -1;
    bool writable;
    std::size_t size = 0;
};
} // namespace Eden
