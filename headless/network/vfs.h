// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "shares.h"
#include "core/file_sys/vfs/vfs.h"

namespace Eden::Network {
FileSys::VirtualFile OpenVfs(std::string_view uri);
FileSys::VirtualDir OpenVfsDirectory(std::string_view uri);
} // namespace Eden::Network
