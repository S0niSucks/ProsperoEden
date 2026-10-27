# SPDX-License-Identifier: GPL-3.0-or-later
add_subdirectory("${EDEN_PORT_DIR}/network" "${PORT_BUILD_DIR}/network")
list(APPEND core_sources "${EDEN_PORT_DIR}/network/vfs.cpp")
target_link_libraries(core PUBLIC eden-network)
string(PREPEND vfs_real_source "#include \"${EDEN_PORT_DIR}/network/vfs.h\"\n")
set(network_open "VirtualFile RealVfsFilesystem::OpenFile(std::string_view path_, OpenMode perms) {")
string(FIND "${vfs_real_source}" "${network_open}" network_open_at)
if(network_open_at LESS 0)
    message(FATAL_ERROR "Pinned RealVfsFilesystem::OpenFile changed; review network VFS routing")
endif()
string(REPLACE "${network_open}" "${network_open}\n    if (::Eden::Network::IsPath(path_))\n        return perms == OpenMode::Read ? ::Eden::Network::OpenVfs(path_) : nullptr;"
    vfs_real_source "${vfs_real_source}")
