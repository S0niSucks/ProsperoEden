// SPDX-License-Identifier: GPL-3.0-or-later
// Real extracted ListEntries + actual Descriptor; Linux syscall adapter only.
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <new>
#include <string>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <vector>
#include "user_content.h"

static_assert(!Eden::UserContent::kIoAvailable, "gate must remain absent/zero");

namespace witness {
enum class Fault { none, buffer, names };
enum class Io { normal, error_first, error_after_batch };
struct State {
    const char* path = nullptr;
    Fault fault = Fault::none;
    Io io = Io::normal;
    int fd = -1, opens = 0, closes = 0, syscalls = 0, positive = 0, eof = 0;
    int injected = 0;
    bool armed = false;
    std::size_t injected_size = 0;
} state;
int controls = 0, regressions = 0, fault_hits = 0;

void* allocate(std::size_t size) {
    auto& s = state;
    const auto target = s.fault == Fault::buffer ? std::size_t(65536) : sizeof(std::string);
    if (s.armed && size == target) {
        s.armed = false;
        ++s.injected;
        s.injected_size = size;
        throw std::bad_alloc();
    }
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void require(bool value, const char* what) {
    if (!value) { ++controls; std::fprintf(stderr, "CONTROL_FAILURE %s\n", what); }
}
} // namespace witness

void* operator new(std::size_t n) { return witness::allocate(n); }
void* operator new[](std::size_t n) { return witness::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

extern "C" int __real_open(const char*, int, ...);
extern "C" int __real_close(int);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    // Extracted ListEntries opens a directory without O_CREAT; this wrapper
    // accepts only that mode. Fixture setup happens in Python, not through it.
    if (flags & O_CREAT) std::abort();
    const int fd = __real_open(path, flags);
    auto& s = witness::state;
    if (fd >= 0 && s.path && std::strcmp(path, s.path) == 0 && (flags & O_DIRECTORY)) {
        s.fd = fd;
        ++s.opens;
        if (s.fault == witness::Fault::buffer) s.armed = true;
    }
    return fd;
}
extern "C" int __wrap_close(int fd) {
    if (fd == witness::state.fd) ++witness::state.closes;
    return __real_close(fd);
}

int sceKernelGetdents(int fd, char* data, int size) {
    // Host-only ABI adapter: real Linux directory entries, no fabricated
    // ListEntries result and no replacement of its filtering/parser/sort.
    static_assert(sizeof(((dirent*)nullptr)->d_reclen) == sizeof(std::uint16_t));
    static_assert(sizeof(((dirent*)nullptr)->d_type) == sizeof(std::uint8_t));
    auto& s = witness::state;
    ++s.syscalls;
    if (s.io == witness::Io::error_first ||
        (s.io == witness::Io::error_after_batch && s.positive > 0)) {
        errno = EIO;
        return -1;
    }
    const long result = ::syscall(SYS_getdents64, fd, data, size);
    if (result > 0) {
        ++s.positive;
        if (s.fault == witness::Fault::names) s.armed = true;
    }
    if (result == 0) ++s.eof;
    return static_cast<int>(result);
}

// Verbatim generated function, hash/line provenance in prepared/provenance.json.
#include "list_entries.inc"

static bool is_closed(int fd) {
    errno = 0;
    const int result = ::fcntl(fd, F_GETFD);
    return result == -1 && errno == EBADF;
}

static bool is_open(int fd) {
    return ::fcntl(fd, F_GETFD) >= 0;
}

static void cleanup_leak() {
    const int fd = witness::state.fd;
    if (fd >= 0 && !is_closed(fd)) (void)__real_close(fd); // after observation only
}

static void allocation_case(const std::string& root, witness::Fault fault, int iteration) {
    using namespace witness;
    state = {};
    state.path = root.c_str();
    state.fault = fault;
    bool threw = false, ok = true;
    try { (void)ListEntries(root, false, ok); }
    catch (const std::bad_alloc&) { threw = true; }
    catch (...) { require(false, "unexpected exception type"); }
    state.armed = false;
    require(threw && state.injected == 1, "exact one-shot real operator new failure");
    require(state.opens == 1 && state.fd >= 0, "successful real directory acquisition");
    require(!ok, "failed enumeration does not report EOF success");
    require(state.injected_size == (fault == Fault::buffer ? 65536 : sizeof(std::string)),
            "fault at buffer / initial names-vector allocation");
    if (fault == Fault::buffer) require(state.syscalls == 0, "buffer failed before getdents");
    else require(state.positive == 1, "names failure after real getdents batch");
    fault_hits += state.injected;
    const bool closed = state.fd >= 0 && is_closed(state.fd);
    const bool leaked = state.fd >= 0 && !closed && is_open(state.fd) && state.closes == 0;
    if (leaked) ++regressions;
    else require(closed && state.closes == 1, "one close plus actual EBADF after unwind");
    std::printf("FD_CASE fault=%s iteration=%d closed=%d close_calls=%d leaked=%d\n",
        fault == Fault::buffer ? "buffer" : "names", iteration, closed, state.closes, leaked);
    cleanup_leak();
}

static void io_case(const std::string& path, bool folders, witness::Io io,
                    const char* expected, bool expected_ok, bool acquisition) {
    using namespace witness;
    state = {};
    state.path = path.c_str();
    state.io = io;
    bool ok = true;
    const auto names = ListEntries(path, folders, ok);
    require(ok == expected_ok, "legacy EOF/error ok semantics");
    require(expected ? names.size() == 1 && names[0] == expected : names.empty(),
            "real directory filtering / link exclusion / partial error outputs");
    require(state.opens == int(acquisition), "control acquisition count");
    if (acquisition) {
        require(state.closes == 1 && is_closed(state.fd), "control close exactly once and EBADF");
        if (io == Io::normal) require(state.eof == 1 && state.positive == 1, "real batch then EOF");
    } else require(state.closes == 0, "no close on failed open");
    cleanup_leak();
}

int main(int argc, char** argv) {
    using namespace witness;
    if (argc != 2) return 20;
    const std::string root = argv[1];
    for (int n = 0; n < 8; ++n) {
        allocation_case(root, Fault::buffer, n);
        allocation_case(root, Fault::names, n);
    }
    io_case(root, false, Io::normal, "abc", true, true);
    io_case(root, true, Io::normal, "sub", true, true);
    io_case(root, false, Io::error_first, nullptr, false, true);
    io_case(root, false, Io::error_after_batch, "abc", false, true);
    io_case(root + "/absent", false, Io::normal, nullptr, false, false);
    require(fault_hits == 16, "all 16 allocation failures were injected");
    std::printf("SUMMARY kind=fd controls=%d regressions=%d fault_hits=%d\n",
                controls, regressions, fault_hits);
    return controls ? 20 : regressions ? 10 : 0;
}
