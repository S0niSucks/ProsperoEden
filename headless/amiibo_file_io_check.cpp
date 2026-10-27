// SPDX-License-Identifier: GPL-3.0-or-later
#include "amiibo_file.h"
#include "native_directory.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include <sys/syscall.h>

namespace {
unsigned read_calls{}, write_calls{};
bool interrupt_read{}, interrupt_write{}, eof{}, fail_write{};
constexpr size_t kChunk = 7;
unsigned directory_fault{};
}
extern "C" ssize_t __real_pread(int, void*, size_t, off_t);
extern "C" ssize_t __real_pwrite(int, const void*, size_t, off_t);
extern "C" ssize_t __wrap_pread(int fd, void* data, size_t size, off_t at) {
    ++read_calls;
    if (interrupt_read) { interrupt_read = false; errno = EINTR; return -1; }
    if (eof) return 0;
    return __real_pread(fd, data, std::min(size, kChunk), at);
}
extern "C" ssize_t __wrap_pwrite(int fd, const void* data, size_t size, off_t at) {
    ++write_calls;
    if (interrupt_write) { interrupt_write = false; errno = EINTR; return -1; }
    if (fail_write && at >= 7) { errno = EIO; return -1; }
    return __real_pwrite(fd, data, std::min(size, kChunk), at);
}
extern "C" int sceKernelGetdents(int fd, char* bytes, int count) {
    assert(count == 65536);
    if (!directory_fault) return static_cast<int>(syscall(SYS_getdents64, fd, bytes, count));
    if (directory_fault == 1) { errno = EIO; return -1; }
    if (directory_fault == 2) return count + 1;
    constexpr auto start = offsetof(dirent, d_name);
    if (directory_fault == 3) return start;
    uint16_t size = start + 5;
    std::memset(bytes, 1, size);
    if (directory_fault == 4) size = start;
    if (directory_fault == 5) size += 100;
    std::memcpy(bytes + offsetof(dirent, d_reclen), &size, sizeof(size));
    if (directory_fault == 7) bytes[start] = 0;
    if (directory_fault == 8) std::memcpy(bytes + start, "a/b", 4);
    return start + 5;
}

int main() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("eden-amiibo-io-" + std::to_string(getpid()));
    fs::create_directories(root / "Mario");
    const auto path = root / "Mario/tag.bin";
    const auto save = [&](size_t size) {
        const std::vector<char> bytes(size, 11);
        std::ofstream(path, std::ios::binary).write(bytes.data(), bytes.size());
    };
    std::array<unsigned char, 540> bytes{};
    for (const auto size : {532u, 540u, 572u}) {
        save(size);
        const Eden::AmiiboFile file(path.string(), true);
        assert(file.IsOpen() && file.GetSize() == size);
        bytes.fill(0); interrupt_read = true; read_calls = 0;
        assert(file.Read(bytes) == std::min(size_t{size}, bytes.size()));
        assert(read_calls > 70 && bytes[0] == 11 && bytes[531] == 11);
        assert(bytes[539] == (size == 532 ? 0 : 11));
        bytes.fill(29); interrupt_write = true; write_calls = 0;
        assert(file.Write(bytes) && write_calls > 70);
        assert(fs::file_size(path) == (size == 572 ? 572 : 540));
        bytes.fill(0); assert(Eden::AmiiboFile(path.string()).Read(bytes) == 540 && bytes[539] == 29);
        if (size == 572) {
            std::ifstream signature(path, std::ios::binary); signature.seekg(540);
            for (unsigned n = 0; n < 32; ++n) assert(signature.get() == 11);
        }
    }
    {
        const Eden::AmiiboFile file(path.string(), true);
        eof = true; read_calls = 0; assert(file.Read(bytes) == 0 && read_calls == 1); eof = false;
        fail_write = true; write_calls = 0; assert(!file.Write(bytes) && write_calls == 2); fail_write = false;
        const auto calls = write_calls;
        assert(!file.Write(std::span<const unsigned char>(bytes).first(532)) && write_calls == calls);
        std::array<unsigned char, 572> too_large{};
        const auto reads = read_calls; assert(file.Read(too_large) == 0 && read_calls == reads);
    }
    {
        const Eden::AmiiboFile readonly(path.string());
        assert(!readonly.Write(bytes));
    }
    fs::create_symlink(path, root / "tag.bin");
    fs::create_directory_symlink(root / "Mario", root / "alias");
    assert(!Eden::AmiiboFile((root / "tag.bin").string()).IsOpen());
    assert(!Eden::AmiiboFile((root / "alias/tag.bin").string(), true).IsOpen());
    assert(!Eden::AmiiboFile((root / "Mario/../Mario/tag.bin").string()).IsOpen());
    assert(!Eden::AmiiboFile(path.string() + std::string("\0suffix", 7)).IsOpen());
    fs::remove(root / "alias");
    const Eden::AmiiboFile held(path.string());
    fs::rename(root / "Mario", root / "original");
    fs::create_directory_symlink(root / "original", root / "Mario");
    assert(!Eden::AmiiboFile(path.string(), true).IsOpen());
    assert(held.Read(bytes) == 540 && bytes[0] == 29);
    const auto listing = root / "listing";
    fs::create_directory(listing);
    for (unsigned n = 0; n < 3; ++n) std::ofstream(listing / (std::to_string(n) + ".txt")) << "test";
    std::error_code error;
    assert(Eden::ReadNativeDirectory(listing, error).size() == 3 && !error);
    for (directory_fault = 0; directory_fault <= 8; ++directory_fault) {
        const int fd = Eden::OpenAmiiboPath(listing.string(), O_RDONLY | O_DIRECTORY);
        assert(fd >= 0);
        const auto entries = Eden::ReadNativeDirectoryFd(fd, listing, error, 1);
        if (directory_fault) assert(entries.empty() && error);
        else assert(entries.size() == 1 && !error);
        assert(fcntl(fd, F_GETFD) >= 0); // the borrowed-descriptor reader must not close its owner
        close(fd);
    }
    fs::remove_all(root);
    std::puts("Amiibo file I/O PASS: short/EINTR/EOF/error operations, 532 padding, full signature "
              "preservation, no-follow ancestors, held descriptors and native directory records");
}
