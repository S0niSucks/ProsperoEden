#!/usr/bin/env python3
"""R332: actual native DualSense edge counter replay and quiet logging contracts."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'headless/pad.cpp').read_text()
header = (root / 'headless/devices.h').read_text()
main = (root / 'headless/main.cpp').read_text()
assert '#include "pad_diagnostics.h"' in header
assert '#include "log_pipe.h"' in source
assert 'face_edges.Observe(last_buttons, pressed, kButtonCross, kButtonCircle,' in source
assert 'kButtonSquare, kButtonTriangle);' in source
assert 'player == 0 && is_usable(raw) && NativeLogs::Detailed()' in source
pad_poll = source.split("bool Pad::Poll() {", 1)[1].split("void Pad::Consume(", 1)[0]
assert 'std::array<ps5::pad::Data, ps5::pad::kMaxSamples> samples;' in pad_poll
assert 'std::array<ps5::pad::Data, ps5::pad::kMaxSamples> samples{};' not in pad_poll
read = 'const int count = scePadRead(handle, samples.data(), samples.size());'
guard = 'if (count < 0 || count > static_cast<int>(samples.size())) {'
consume = 'Consume(player, {samples.data(), static_cast<std::size_t>(count)});'
assert pad_poll.count(read) == pad_poll.count(guard) == pad_poll.count(consume) == 1
assert pad_poll.index(read) < pad_poll.index(guard) < pad_poll.index(
    'for (int i = 0; i < count; ++i)') < pad_poll.index(consume)
assert 'Consume(player, {&neutral, 1});' in pad_poll[
    pad_poll.index(guard):pad_poll.index('for (int i = 0; i < count; ++i)')]
assert 'continue;' in pad_poll[
    pad_poll.index(guard):pad_poll.index('for (int i = 0; i < count; ++i)')]
# The only indexed reads use i < count; the count=0/error path must
# never inspect uninitialized tail bytes.
assert pad_poll.count('samples[i]') == 4
assert pad_poll.index('samples[i]') > pad_poll.index('for (int i = 0; i < count; ++i)')

assert 'if (NativeLogs::Detailed() && polls % 1250 == 0)' in source
assert 'if (polls % 1250 == 0) Performance::ReportInputThreadGpuLiveness();' in source
assert source.index('if (polls % 1250 == 0) Performance::ReportInputThreadGpuLiveness();') < source.index('if (NativeLogs::Detailed() && polls % 1250 == 0)')
assert source.count('EDEN_PAD_HEARTBEAT polls=') == 1
assert 'last_read=%d' in source and 'square=%llu triangle=%llu' in source
assert '"epoch_ms=%lld mono_ns=%lld\\n"' in source
assert source.count('clock_gettime(') == 2
assert source.index('const int utc_rc = clock_gettime(') > source.index('if (NativeLogs::Detailed() && polls % 1250 == 0)')
assert 'epoch_ms, mono_ns);' in source
assert 'ActiveMapping()[game_x], ActiveMapping()[game_y]' in source
assert 'std::fflush(stderr)' not in source
assert 'EDEN_PAD_INPUT mode=%s' in main and 'pad->Poll();' in main
assert 'pad_square, pad_triangle' in (root / 'headless/button_mapping.h').read_text()
# A new title must not inherit the previous renderer's stall timestamp.
perf_source = (root / 'headless/performance.cpp').read_text()
perf_header = (root / 'headless/performance.h').read_text()
assert 'void ResetGpuReportForTitle() noexcept;' in perf_header
assert 'void ResetGpuReportForTitle() noexcept {' in perf_source
assert 'latest_gpu_report_ns.store(0, std::memory_order_release);' in perf_source
assert 'last_gpu_stall_warn_ns.store(0, std::memory_order_relaxed);' in perf_source
assert 'detail=not_measured' in perf_source
assert perf_source.index('latest_gpu_report_frame.store(frame, std::memory_order_relaxed);') < perf_source.index('if (detailed_gpu_profile.load(std::memory_order_relaxed)) {', perf_source.index('void ReportGpuThread(unsigned frame)'))
assert perf_source.index('if (progress_ns == 0) return;', perf_source.index('void ReportInputThreadGpuLiveness()')) < perf_source.index('const auto now = static_cast<unsigned long long>', perf_source.index('void ReportInputThreadGpuLiveness()'))
assert 'Eden::Performance::ResetGpuReportForTitle();' in main

unit = r'''
#include "pad_diagnostics.h"
#include "ps5_pad.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <type_traits>
int main() {
    static_assert(std::is_trivially_default_constructible_v<ps5::pad::Data>);
    static_assert(sizeof(ps5::pad::Data) * ps5::pad::kMaxSamples == 7680);
    // The real platform writes exactly the reported prefix. A zero/invalid
    // count must not expose stale data left by the previous controller.
    std::array<ps5::pad::Data, ps5::pad::kMaxSamples> scratch;
    for (int count : {64, 0, 3, -1, 65, 1}) {
        const bool valid = count >= 0 && count <= int(scratch.size());
        if (!valid) {
            const auto neutral = ps5::pad::neutral_data();
            assert(neutral.left_stick.x == 128);
            continue;
        }
        for (int i = 0; i < count; ++i) {
            scratch[std::size_t(i)] = ps5::pad::neutral_data(std::uint64_t(i + 1));
            scratch[std::size_t(i)].buttons = std::uint32_t(i);
        }
        for (int i = 0; i < count; ++i)
            assert(scratch[std::size_t(i)].buttons == std::uint32_t(i) &&
                   scratch[std::size_t(i)].timestamp_us == std::uint64_t(i + 1));
    }
    Eden::PadEdgeDiagnostics edges{};
    constexpr unsigned cross=1, circle=2, square=4, triangle=8;
    edges.Observe(0, 4, cross, circle, square, triangle);
    assert(edges.changes==1 && edges.square==1 && edges.triangle==0);
    edges.Observe(4, 4, cross, circle, square, triangle);
    assert(edges.changes==1 && edges.square==1);  // held does not recount
    edges.Observe(4, 12, cross, circle, square, triangle);
    assert(edges.changes==2 && edges.square==1 && edges.triangle==1);
    edges.Observe(12, 0, cross, circle, square, triangle);
    assert(edges.changes==3 && edges.square==1 && edges.triangle==1);
    edges.Observe(0, 3, cross, circle, square, triangle);
    assert(edges.changes==4 && edges.cross==1 && edges.circle==1);
    edges.Observe(3, 0, cross, circle, square, triangle);
    edges.Observe(0, 4, cross, circle, square, triangle);
    assert(edges.square==2 && edges.triangle==1 && edges.cross==1 && edges.circle==1);
    std::puts("R332 PASS: physical face rising edges, held/release, independent X/Y accounting");
}
'''
with tempfile.TemporaryDirectory(prefix='eden-r332-pad-') as tmp:
    path = Path(tmp)
    (path / 'test.cpp').write_text(unit)
    env = os.environ.copy()
    if sys.platform == 'darwin' and not env.get('SDKROOT'):
        sdk = Path('/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk')
        if sdk.is_dir(): env['SDKROOT'] = str(sdk)
    for opt in ('0', '2'):
        exe = path / ('test-O' + opt)
        subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-O' + opt,
                        '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-I', str(root/'headless'), '-I', str(root/'third_party'),
                        str(path/'test.cpp'), '-o', str(exe)],
                       check=True, env=env, timeout=30)
        result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=15)
        assert result.returncode == 0, (opt, result.stderr)
        print(result.stdout.strip())
print('R332 PASS: release+dev GPU stall monitor every 1250 polls; detailed pad logging optional, no per-sample I/O, pad_read_prefix_only=1 O0_O2_ASAN_UBSAN=1')
