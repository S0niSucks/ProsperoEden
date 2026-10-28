#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Docker-only configure/header checks against the pinned Eden tree; no emulator build.

EDEN_NETWORK_DEPS must contain the hash-verified trees in tools/deps.json (eden, fmt,
libsmb2, libnfs). An optional PS5_PAYLOAD_SDK adds adapter syntax, client cross-build
and an isolated SDK link check (the resulting ELF is never executed).
"""
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    if not Path("/.dockerenv").exists(): raise SystemExit("Run this focused check inside Docker")
    deps = Path(os.environ.get("EDEN_NETWORK_DEPS", "/work/deps"))
    eden = deps / "mirror-67bada77f8a43a90da2e94e89b8e7da73c256989"
    work = Path(os.environ.get("NETWORK_WORK", "/work/network-check")) / "integration"
    work.mkdir(parents=True, exist_ok=True)

    # Exercise the exact incumbent development instrumentation, not a second approximation.
    incumbent = (ROOT / "headless/CMakeLists.txt").read_text()
    start = incumbent.index('    file(READ "${PROJECT_SOURCE_DIR}/src/core/hle/service/filesystem/fsp/fs_i_file.cpp"')
    end = incumbent.index("    # Open/entry results", start)
    instrumentation = incumbent[start:end] + '''
    write_derived("${PORT_BUILD_DIR}/fs_i_storage.cpp" "#include \\\"${EDEN_PORT_DIR}/performance.h\\\"\\n${fs_storage_source}")
    list(REMOVE_ITEM core_sources "hle/service/filesystem/fsp/fs_i_storage.cpp")
    list(APPEND core_sources "${PORT_BUILD_DIR}/fs_i_storage.cpp")
'''
    project = '''cmake_minimum_required(VERSION 3.24)
project(network_integration LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(EDEN_PORT_DIR "@ROOT@/headless")
set(PROJECT_SOURCE_DIR "@EDEN@")
set(PORT_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/derived")
file(MAKE_DIRECTORY "${PORT_BUILD_DIR}")
function(write_derived path content)
    file(WRITE "${path}" "${content}")
endfunction()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/stub.cpp" "// configure-only target\\n")
add_library(core STATIC "${CMAKE_CURRENT_BINARY_DIR}/stub.cpp")
set(core_sources file_sys/vfs/vfs_real.cpp hle/service/filesystem/fsp/fs_i_file.cpp hle/service/filesystem/fsp/fs_i_storage.cpp)
file(READ "${PROJECT_SOURCE_DIR}/src/core/file_sys/vfs/vfs_real.cpp" vfs_real_source)
include("${EDEN_PORT_DIR}/network/inject.cmake")
write_derived("${PORT_BUILD_DIR}/vfs_real.cpp" "${vfs_real_source}")
list(REMOVE_ITEM core_sources "file_sys/vfs/vfs_real.cpp")
list(APPEND core_sources "${PORT_BUILD_DIR}/vfs_real.cpp")
if(NETWORK_INSTRUMENTED)
@INSTRUMENTATION@
endif()
include("${EDEN_PORT_DIR}/network/guest_io.cmake")
set_property(TARGET core PROPERTY SOURCES "${core_sources}")
get_target_property(final_sources core SOURCES)
if(NOT "${EDEN_PORT_DIR}/network/vfs.cpp" IN_LIST final_sources)
    message(FATAL_ERROR "Network adapter was lost when core SOURCES were replaced")
endif()
foreach(kind file storage)
    if(NOT "${PORT_BUILD_DIR}/network-${kind}.cpp" IN_LIST final_sources)
        message(FATAL_ERROR "Missing network ${kind} overlay")
    endif()
    file(READ "${PORT_BUILD_DIR}/network-${kind}.cpp" content)
    string(REGEX MATCHALL "::Eden::Network::GameError" guards "${content}")
    list(LENGTH guards count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "Expected exactly one read-error guard for ${kind}")
    endif()
    foreach(source IN LISTS final_sources)
        if(source MATCHES "(^|/)fs_i_${kind}\\\\.cpp$")
            message(FATAL_ERROR "Unguarded ${kind} handler remained in core")
        endif()
    endforeach()
endforeach()
string(FIND "${vfs_real_source}" "perms == OpenMode::Read ? ::Eden::Network::OpenVfs(path_) : nullptr" route)
if(route LESS 0)
    message(FATAL_ERROR "Missing read-only VFS route")
endif()
'''
    (work / "CMakeLists.txt").write_text(project.replace("@ROOT@", str(ROOT)).replace("@EDEN@", str(eden))
                                        .replace("@INSTRUMENTATION@", instrumentation))
    for mode in ("OFF", "ON"):
        subprocess.run(["cmake", "-S", str(work), "-B", str(work / mode),
                        f"-DEDEN_NETWORK_DEPS={deps}", f"-DNETWORK_INSTRUMENTED={mode}"], check=True)
    includes = [f"-I{ROOT / 'headless'}", f"-I{eden / 'src'}", f"-I{deps / 'fmt-12.1.0/include'}"]
    adapter = str(ROOT / "headless/network/vfs.cpp")
    subprocess.run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", *includes, "-fsyntax-only", adapter], check=True)
    if sdk := os.environ.get("PS5_PAYLOAD_SDK"):
        compiler = ["clang++-18", "--target=x86_64-sie-ps5", "-std=c++20", "-isysroot", sdk,
                    "-isystem", f"{sdk}/target/include/c++/v1", "-isystem", f"{sdk}/target/include",
                    *includes, "-fexceptions", "-fcxx-exceptions"]
        for source in (adapter, str(ROOT / "headless/network/metadata.cpp")):
            subprocess.run([*compiler, "-fsyntax-only", source], check=True)
        # Only JSON's architecture-independent headers may come from the host. Never put
        # /usr/include on an SDK compilation's search path (it would mix libc ABIs).
        shutil.copytree("/usr/include/nlohmann", work / "include/nlohmann", dirs_exist_ok=True)
        env = {**os.environ, "CXXFLAGS": f"-I{work / 'include'}"}
        cross = work / "sdk"
        subprocess.run(["cmake", "-S", str(ROOT / "headless/network"), "-B", str(cross),
                        f"-DCMAKE_TOOLCHAIN_FILE={ROOT / 'tools/ps5.cmake'}", f"-DEDEN_NETWORK_DEPS={deps}"],
                       check=True, env=env)
        subprocess.run(["cmake", "--build", str(cross), "--target", "eden-network", "-j2"], check=True)
        smoke = work / "sdk-smoke.cpp"
        smoke.write_text('''#include "network/shares.h"
#include "network/network.h"
#include "network/editor.h"
int main() {
    Eden::Network::Editor editor;
    std::atomic<bool> stop{true};
    editor.Open("/data/synthetic/config/network-shares.json");
    editor.Begin(""); editor.Enter(Eden::Network::Field::password, stop);
    editor.Save(stop); editor.Remove("synthetic", stop); editor.Cancel();
    Eden::Network::Configure("/data/synthetic-network-settings.json");
    Eden::Network::BeginGame();
    auto file = Eden::Network::Open("pe-net://test/Game.nsp");
    if (file.session) { unsigned char bytes[32]; file.session->Read(file.entry, bytes, 32, 0); }
    Eden::Network::CancelGame(); Eden::Network::EndGame(); Eden::Network::Disconnect();
}
''')
        obj = work / "sdk-smoke.o"
        subprocess.run([*compiler, "-femulated-tls", "-c", str(smoke), "-o", str(obj)], check=True)
        keyboard = work / "sdk-native-keyboard.o"
        subprocess.run([*compiler, "-femulated-tls", "-DPS5_NATIVE=1", "-c", str(ROOT / 'headless/system_keyboard.cpp'),
                        "-o", str(keyboard)], check=True)
        # The project's existing import facade, built only into the private evidence directory.
        common_obj = work / 'common-dialog.o'
        common_so = work / 'libSceCommonDialog.so'
        subprocess.run([f"{sdk}/bin/prospero-clang", "-fPIC", "-c", str(ROOT / 'tools/stubs/libSceCommonDialog.c'),
                        "-o", str(common_obj)], check=True)
        subprocess.run([f"{sdk}/bin/prospero-lld", '--shared', '-soname', 'libSceCommonDialog.sprx', '-o',
                        str(common_so), str(common_obj)], check=True)
        subprocess.run([f"{sdk}/bin/prospero-clang++", "-lkernel", str(obj), str(keyboard), "-Wl,--start-group",
                        str(cross / "libeden-network.a"), str(cross / "smb2/lib/libsmb2.a"),
                        str(cross / "nfs/lib/libnfs.a"), "-Wl,--end-group", str(common_so),
                        f"{sdk}/target/lib/libSceImeDialog.so", f"{sdk}/target/lib/libSceSysmodule.so",
                        f"{sdk}/target/lib/libSceUserService.so", "-o", str(work / "sdk-smoke.elf")], check=True)
        print("SDK transport/private editor/atomic config/native keyboard cross-build and isolated import-facade link PASS (ELF not executed)")
    print("Pinned Eden VFS headers, adapter retention, plain/instrumented IPC error guards PASS")


if __name__ == "__main__": main()
