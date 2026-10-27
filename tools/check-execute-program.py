#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the frontend's ExecuteProgram callback and relaunch logic (upstream PR 298).

Uses stand-ins for Eden's applet parameters, not a commercial game or a PS5. Checks program
index zero, competing requests, previous-program metadata, return-to-menu, and cycle accounting.
"""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
main = (root / 'headless/main.cpp').read_text()
start = main.index('                struct Completion {')
completion = main[start:main.index('\n                auto completion', start)]
start = main.index('                system.RegisterExecuteProgramCallback([completion]')
callback = main[start:main.index('\n                SCOPE_EXIT', start)]
start = main.index('                std::optional<std::size_t> program_asked;')
selection = main[start:main.index('\n                system.ShutdownMainProcess();', start)]
start = main.index('                if (program_asked && !return_to_menu) {')
relaunch = main[start:main.index('\n            }\n        }\n        passed("core_destroyed");', start)]
assert 'system.RegisterExecuteProgramCallback({});' in main
assert main.index('std::optional<Service::AM::FrontendAppletParameters> next_program;') < main.index('for (unsigned cycle')
assert 'params = *next_program;\n                    next_program.reset();' in main
assert main.index('if (program_asked) previous_program_id') < main.index('system.ShutdownMainProcess();', main.index('std::optional<std::size_t> program_asked;'))

code = r'''
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
using u64 = std::uint64_t;
using s32 = std::int32_t;
#define LOG_INFO(...) ((void)0)
namespace Service::AM {
enum class AppletId { Application };
enum class LaunchType { User, ApplicationInitiated };
enum class AppletProgramId : u64 { MaxProgramId = 0x1fff };
struct FrontendAppletParameters {
    AppletId applet_id = AppletId::Application;
    LaunchType launch_type = LaunchType::User;
    s32 program_index = 0;
    u64 program_id = 0;
    s32 previous_program_index = -1;
};
}
namespace FileSys { u64 GetBaseTitleID(u64 id) { return id & ~u64{0xfff}; } }
struct System {
    std::function<void(std::size_t)> callback;
    u64 id = 0;
    bool stopped = false;
    void RegisterExecuteProgramCallback(std::function<void(std::size_t)> fn) { callback = std::move(fn); }
    u64 GetApplicationProcessProgramID() const { assert(!stopped); return id; }
    void ShutdownMainProcess() { stopped = true; }
};
''' + completion + r'''
static void Register(System& system, std::shared_ptr<Completion> completion) {
''' + callback + r'''
}
static std::optional<Service::AM::FrontendAppletParameters> Transition(
        System& system, std::shared_ptr<Completion> completion, bool return_to_menu, unsigned& cycle) {
    std::optional<Service::AM::FrontendAppletParameters> next_program;
''' + selection + r'''
    system.ShutdownMainProcess();
''' + relaunch + r'''
    return next_program;
}
int main() {
    // Zero is a valid requested index, not an empty request. First request wins, even if zero.
    {
        System system;
        system.id = 0x0100123456789002;
        auto completion = std::make_shared<Completion>();
        Register(system, completion);
        std::thread guest([&] { system.callback(0); system.callback(7); });
        guest.join();
        assert(completion->exited && completion->execute_program == 0);
        unsigned cycle = 0;
        auto next = Transition(system, completion, false, cycle);
        assert(system.stopped && next && next->program_index == 0);
        assert(next->launch_type == Service::AM::LaunchType::ApplicationInitiated);
        assert(next->program_id == system.id && next->previous_program_index == 2);
        ++cycle;
        assert(cycle == 0); // program changes do not spend --repeat/--soak cycles
    }
    // A user leaving the game cancels its pending handover, before or after shutdown.
    for (bool before : {false, true}) {
        System system;
        auto completion = std::make_shared<Completion>();
        Register(system, completion);
        system.callback(3);
        completion->return_to_menu = before;
        unsigned cycle = 1;
        assert(!Transition(system, completion, !before, cycle));
        assert(cycle == 1);
    }
    // No ExecuteProgram leaves a normal session alone, and a system applet has no title metadata.
    {
        System system;
        auto completion = std::make_shared<Completion>();
        unsigned cycle = 0;
        assert(!Transition(system, completion, false, cycle));
        assert(cycle == 0);
        system.stopped = false;
        Register(system, completion);
        system.callback(4);
        auto next = Transition(system, completion, false, cycle);
        assert(next && next->program_index == 4 && next->program_id == 0);
        assert(next->previous_program_index == -1);
    }
    // Multiple program changes stay in the same session and stop when no new program is asked for.
    unsigned loads = 0;
    for (unsigned cycle = 0; cycle < 1; ++cycle) {
        assert(++loads <= 3);
        System system;
        auto completion = std::make_shared<Completion>();
        Register(system, completion);
        if (loads < 3) system.callback(loads);
        auto next = Transition(system, completion, false, cycle);
        assert(next.has_value() == (loads < 3));
    }
    assert(loads == 3);
    std::puts("ExecuteProgram: PASS callback, program zero, cancellation, metadata, cycle accounting");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    exe = Path(tmp) / 'execute-program'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-pthread', '-fsanitize=undefined', '-x', 'c++', '-', '-o', str(exe)],
                   input=code, text=True, check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
