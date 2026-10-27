// SPDX-License-Identifier: GPL-3.0-or-later
// Compile the feature against the real service declaration without building emulator/core code.
#include "prosperoeden/eden_services.h"
#include "assets_dir.h"
#include "update_notice.h"
#include <algorithm>

// Collaborators defined before the feature include in eden_services.cpp.
bool WriteCover(const std::string&, const std::string&);
std::vector<std::string> ListFolder(const std::string&);
std::vector<std::pair<std::string, std::string>> ReadPending();

#include "prosperoeden/usb_backup_services.inc"
