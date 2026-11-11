// SPDX-License-Identifier: GPL-3.0-or-later
// Docker/Linux host helper fixture, not native/runtime qualification.
#include "user_content.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
enum class Fault { none, first_read, last_read, final_stat, read_error, stat_error, interrupted_read };
Fault fault = Fault::none;
bool armed = false;
std::atomic<bool> cancel{false};
std::size_t expected_size = 0, read_bytes = 0;
unsigned stat_calls = 0, read_calls = 0;
int image_fd = -1, directory_fd = -1;
unsigned failures = 0, regressions = 0;

void Require(bool condition, const char* label) {
    if (!condition) { ++failures; std::fprintf(stderr, "A05_CONTROL_FAIL: %s\n", label); }
}

bool Closed(int fd) {
    errno = 0;
    return fd >= 0 && ::fcntl(fd, F_GETFD) == -1 && errno == EBADF;
}

void Check(const std::string& folder, const char* name, std::size_t size, Fault injected,
           bool success, bool pre_cancelled = false, bool use_token = true) {
    fault = injected;
    expected_size = size;
    read_bytes = 0;
    stat_calls = read_calls = 0;
    image_fd = directory_fd = -1;
    cancel.store(pre_cancelled, std::memory_order_release);
    std::vector<unsigned char> output{1, 2, 3}; // no stale outputs on any refusal
    armed = true;
    const bool result = Eden::UserContent::ReadImage(folder, name, output, use_token ? &cancel : nullptr);
    armed = false;
    const bool terminal_cancel = injected == Fault::last_read || injected == Fault::final_stat;
    if (terminal_cancel) {
        Require(cancel.load() && read_bytes == size && stat_calls == 2, "terminal injection reached actual I/O");
        if (result || !output.empty()) {
            ++regressions;
            std::fprintf(stderr, "A05_TERMINAL_CANCEL_RETURNED_BYTES: fault=%d size=%zu\n", int(injected), size);
        }
    } else {
        Require(result == success, "expected success/refusal");
        Require(success ? output == std::vector<unsigned char>(size, 42) : output.empty(), "complete or empty bytes");
    }
    if (image_fd >= 0) Require(Closed(image_fd), "image descriptor closed before return");
    if (directory_fd >= 0) Require(Closed(directory_fd), "directory descriptor closed before return");
}
} // namespace

extern "C" ssize_t __real_read(int, void*, std::size_t);
extern "C" int __real_fstat(int, struct stat*);
extern "C" int __real_fstatat(int, const char*, struct stat*, int);

extern "C" ssize_t __wrap_read(int fd, void* data, std::size_t count) {
    if (!armed) return __real_read(fd, data, count);
    ++read_calls;
    if (fault == Fault::read_error) { errno = EIO; return -1; }
    if (fault == Fault::interrupted_read && read_calls == 1) { errno = EINTR; return -1; }
    const auto result = __real_read(fd, data, count);
    if (result > 0) {
        read_bytes += static_cast<std::size_t>(result);
        if ((fault == Fault::first_read && read_calls == 1) ||
            (fault == Fault::last_read && read_bytes == expected_size))
            cancel.store(true, std::memory_order_release);
    }
    return result;
}

extern "C" int __wrap_fstat(int fd, struct stat* st) {
    const int result = __real_fstat(fd, st);
    if (armed) {
        image_fd = fd;
        ++stat_calls;
        if (stat_calls == 2 && fault == Fault::final_stat)
            cancel.store(true, std::memory_order_release);
        if (stat_calls == 2 && fault == Fault::stat_error) { errno = EIO; return -1; }
    }
    return result;
}

extern "C" int __wrap_fstatat(int fd, const char* name, struct stat* st, int flags) {
    if (armed) directory_fd = fd;
    return __real_fstatat(fd, name, st, flags);
}

int main() {
    namespace fs = std::filesystem;
    std::array<char, 40> pattern{};
    const std::string prefix = "/tmp/encore-a05-XXXXXX";
    std::copy(prefix.begin(), prefix.end(), pattern.begin());
    const char* created = ::mkdtemp(pattern.data());
    if (!created) return 20;
    const std::string folder{created}; // only this exclusively created synthetic fixture is removed
    struct Cleanup { std::string path; ~Cleanup() { std::error_code ec; fs::remove_all(path, ec); } } cleanup{folder};
    const auto Write = [&](const char* name, std::size_t size) {
        const std::vector<unsigned char> bytes(size, 42);
        std::ofstream stream(folder + "/" + name, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        Require(bool(stream), "synthetic fixture creation");
    };
    Write("small.tga", 18);
    Write("chunks.tga", 65537);
    for (const auto& [name, size] : {std::pair{"small.tga", std::size_t{18}},
                                     std::pair{"chunks.tga", std::size_t{65537}}}) {
        Check(folder, name, size, Fault::none, true);
        Check(folder, name, size, Fault::none, true, false, false); // nullptr token still succeeds
        Check(folder, name, size, Fault::none, false, true);
        Check(folder, name, size, Fault::last_read, false);
        Check(folder, name, size, Fault::final_stat, false);
        Check(folder, name, size, Fault::read_error, false);
        Check(folder, name, size, Fault::stat_error, false);
        Check(folder, name, size, Fault::interrupted_read, true);
    }
    Check(folder, "chunks.tga", 65537, Fault::first_read, false);
    fs::create_symlink(folder + "/small.tga", folder + "/link.tga");
    fs::create_directory_symlink(folder, folder + "/alias");
    Check(folder, "link.tga", 18, Fault::none, false);
    Check(folder + "/alias", "small.tga", 18, Fault::none, false);
    fs::create_hard_link(folder + "/small.tga", folder + "/hard.tga");
    Check(folder, "small.tga", 18, Fault::none, false);
    fs::remove(folder + "/hard.tga");
    Require(::mkfifo((folder + "/pipe.png").c_str(), 0600) == 0, "synthetic FIFO creation");
    Check(folder, "pipe.png", 0, Fault::none, false);
    Write("short.tga", 17);
    Check(folder, "short.tga", 17, Fault::none, false);
    Write("large.tga", 18);
    fs::resize_file(folder + "/large.tga", Eden::UserContent::kMaxImageBytes + 1);
    Check(folder, "large.tga", Eden::UserContent::kMaxImageBytes + 1, Fault::none, false);
    Check(folder, "../small.tga", 18, Fault::none, false);
    std::vector<std::string> folders{"stale"}, images{"stale"};
    cancel.store(true);
    Require(!Eden::UserContent::Entries(folder, &folders, &images, &cancel) && folders.empty() && images.empty(),
            "cancelled discovery outputs still empty");
    if (failures) return 20; // unrelated control failure must not be mistaken for baseline reproduction
    if (regressions == 4) { std::puts("A05_READ_IMAGE_CANCEL_REGRESSION"); return 10; }
    if (regressions) return 20;
    std::puts("A05_READ_IMAGE_CANCEL_PASS");
    return 0;
}
