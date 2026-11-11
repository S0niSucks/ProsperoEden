#!/usr/bin/env python3
"""Compile real source-extracted fastmem install/range guards with mock signal API."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
s=(root/"headless/fastmem_handler.cpp").read_text()
start="constexpr bool ValidCodeRange("
end="\nvoid Forward("
assert s.count(start)==s.count(end)==1
range_helper=start+s.split(start,1)[1].split(end,1)[0]
install="bool Install() {"+s.split("bool Install() {",1)[1].split("\n} // namespace",1)[0]
assert s.count("bool Install() {")==1
assert s.count("if (!impl || impl->range || !ValidCodeRange(impl->begin, impl->size) || !Install()) return;")==1
assert 'sigaction(SIGSEGV, &previous_segv, nullptr)' in install
assert install.index('sigaction(SIGSEGV, &previous_segv, nullptr)') > install.index('sigaction(SIGBUS, &action, &previous_bus)')
compiler=shutil.which("clang++-18") or shutil.which("clang++")
assert compiler
fixture=r"""
#include <cassert>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <limits>
__RANGE__
static std::once_flag install_once;
static bool installed=false;
static struct sigaction previous_segv{}, previous_bus{};
static struct sigaction current_segv{},current_bus{};
static bool bus_failure=false;
static unsigned segv_installs=0,segv_restores=0,bus_installs=0;
void Handle(int, siginfo_t*, void*) {}
int MockSigaction(int which,const struct sigaction* requested,struct sigaction* previous) {
    assert(requested);
    if(which==SIGSEGV) {
        if(previous) {
            *previous=current_segv;
            ++segv_installs;
        } else {
            ++segv_restores;
        }
        current_segv=*requested;
        return 0;
    }
    assert(which==SIGBUS);
    ++bus_installs;
    if(bus_failure) return -1;
    if(previous) *previous=current_bus;
    current_bus=*requested;
    return 0;
}
#define sigaction(...) MockSigaction(__VA_ARGS__)
__INSTALL__
#undef sigaction
int main(int argc,char** argv) {
    assert(argc==2);
    assert(ValidCodeRange(1,1));
    assert(ValidCodeRange(100,200));
    assert(!ValidCodeRange(0,1));
    assert(!ValidCodeRange(~std::uint64_t{0},1));
    assert(!ValidCodeRange(1,0));
    assert(!ValidCodeRange(~std::uint64_t{0}-7,8));
    assert(ValidCodeRange(~std::uint64_t{0}-8,8));
    current_segv.sa_handler=SIG_IGN;
    current_bus.sa_handler=SIG_DFL;
    bus_failure=argv[1][0]=='f';
    const bool ok=Install();
    assert(ok==!bus_failure);
    assert(Install()==ok);
    assert(segv_installs==1 && bus_installs==1);
    if(bus_failure) {
        assert(segv_restores==1);
        assert(current_segv.sa_handler==SIG_IGN);
        assert(current_bus.sa_handler==SIG_DFL);
    } else {
        assert(segv_restores==0);
        assert(current_segv.sa_sigaction==Handle);
        assert(current_bus.sa_sigaction==Handle);
    }
}
""".replace("__RANGE__",range_helper).replace("__INSTALL__",install)
with tempfile.TemporaryDirectory(prefix="eden-fastmem-install-") as tmp:
    d=Path(tmp);cpp=d/"signal.cpp";exe=d/"signal"
    cpp.write_text(fixture)
    subprocess.run([compiler,"-std=c++20","-O1","-Wall","-Wextra","-Werror",
                    "-fsanitize=address,undefined","-fno-sanitize-recover=all",
                    str(cpp),"-o",str(exe)],check=True,timeout=90)
    for mode in ("failed_bus","success"):
        subprocess.run([str(exe),mode],check=True,timeout=20,capture_output=True)
print("FASTMEM_INSTALL_HOST_PASS both_or_neither=1 rollback=1 wrapped_ranges_rejected=1 sanitizer=1")


# Compile the exact signal-time bounded scan and publication, with a
# read-counted mock atomic: no empty-slot work with zero/one/few JIT arenas.
handler_anchor = "void Handle(int signal, siginfo_t* info, void* context) {"
handler_end = "\nbool Install() {"
assert s.count(handler_anchor) == s.count(handler_end) == 1
handle = handler_anchor + s.split(handler_anchor, 1)[1].split(handler_end, 1)[0]
pub_anchor = "            const std::size_t required_prefix ="
pub_end = "            impl->range = &range;"
assert s.count(pub_anchor) == s.count(pub_end) == 1
publish = pub_anchor + s.split(pub_anchor, 1)[1].split(pub_end, 1)[0]
assert "published_range_prefix.load(std::memory_order_acquire)" in handle
assert "i < prefix" in handle and "ranges[i]" in handle
assert "compare_exchange_weak" in publish
assert s.index("range.begin.store(impl->begin, std::memory_order_release)") < s.index(pub_anchor)
scan_fixture = r"""
#include <array>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <functional>
using u64 = std::uint64_t;
struct FakeCall { u64 ret_rip; u64 call_rip; };
std::atomic<unsigned> reads{0};
struct CountingBegin {
    std::atomic<u64> value{0};
    u64 load(std::memory_order order) const noexcept {
        reads.fetch_add(1, std::memory_order_relaxed);
        return value.load(order);
    }
    void store(u64 n, std::memory_order order) noexcept { value.store(n, order); }
};
struct CodeRange {
    CountingBegin begin;
    std::atomic<u64> end{0};
    std::atomic<const std::function<FakeCall(u64)>*> callback{nullptr};
};
std::array<CodeRange, 64> ranges{};
std::atomic<std::size_t> published_range_prefix{0};
std::atomic<u64> fault_count{0};
std::atomic<unsigned> forwarded{0};
namespace Eden::Fastmem {
struct Context { u64 pc; u64 sp; };
u64& ContextRip(void* p) noexcept { return static_cast<Context*>(p)->pc; }
u64& ContextRsp(void* p) noexcept { return static_cast<Context*>(p)->sp; }
}
void Forward(int, siginfo_t*, void*) { forwarded.fetch_add(1); }
__HANDLE__
void Publish(unsigned slot, u64 begin, std::function<FakeCall(u64)>& callback) {
    assert(slot < ranges.size());
    auto& range = ranges[slot];
    range.end.store(begin + 64, std::memory_order_relaxed);
    range.callback.store(&callback, std::memory_order_release);
    range.begin.store(begin, std::memory_order_release);
__PUBLISH__
}
int main() {
    std::function<FakeCall(u64)> callback = [](u64) { return FakeCall{0xFADE, 0xFACE}; };
    std::array<u64, 8> stack{};
    Eden::Fastmem::Context context{0x1000, reinterpret_cast<u64>(stack.data() + stack.size())};
    Handle(SIGSEGV, nullptr, &context);
    assert(reads.load() == 0 && forwarded.load() == 1); // no registered ranges
    Publish(0, 0x1000, callback);
    assert(published_range_prefix.load() == 1);
    reads.store(0);
    Handle(SIGSEGV, nullptr, &context);
    assert(reads.load() == 1 && forwarded.load() == 1 && fault_count.load() == 1);
    assert(context.pc == 0xFACE && stack.back() == 0xFADE);
    context.pc = 0x2000;
    reads.store(0);
    Handle(SIGBUS, nullptr, &context);
    assert(reads.load() == 1 && forwarded.load() == 2);
    Publish(3, 0x3000, callback); // a sparse high-index registration
    assert(published_range_prefix.load() == 4);
    context.pc = 0x3005;
    reads.store(0);
    Handle(SIGSEGV, nullptr, &context);
    assert(reads.load() == 4 && fault_count.load() == 2);
    ranges[3].callback.store(nullptr, std::memory_order_release);
    ranges[3].begin.store(0, std::memory_order_release);
    context.pc = 0x3005;
    reads.store(0);
    Handle(SIGBUS, nullptr, &context);
    assert(reads.load() == 4 && forwarded.load() == 3);
    Publish(3, 0x4000, callback); // slot reuse, bound never shrinks
    assert(published_range_prefix.load() == 4);
    context.pc = 0x4002;
    Handle(SIGSEGV, nullptr, &context);
    assert(fault_count.load() == 3 && context.pc == 0xFACE);
}
""".replace("__HANDLE__", handle).replace("__PUBLISH__", publish)
with tempfile.TemporaryDirectory(prefix="eden-fastmem-prefix-") as tmp:
    d = Path(tmp)
    cpp = d / "signal-prefix.cpp"
    cpp.write_text(scan_fixture)
    for opt in (0, 2):
        exe = d / f"signal-O{opt}"
        subprocess.run([compiler, "-std=c++20", f"-O{opt}", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        str(cpp), "-o", str(exe)], check=True, timeout=90)
        subprocess.run([str(exe)], check=True, timeout=30)
print("FASTMEM_PREFIX_HOST_PASS empty=0 one=1 sparse_slot4=4 retirement=1 reuse=1 O0_O2=1 ASan_UBSan=1")
