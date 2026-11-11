// SPDX-License-Identifier: GPL-3.0-or-later
// Host-compiled PS5_NATIVE header path only; not a native ABI qualification.
#include "user_content.h"
#include <cstdio>

int main() {
    static_assert(!Eden::UserContent::kIoAvailable);
    std::vector<unsigned char> bytes{1};
    std::vector<std::string> folders{"old"}, images{"old"};
    if (Eden::UserContent::OpenDirectory("/data/synthetic-a05") != -1 ||
        Eden::UserContent::ReadImage("/data/synthetic-a05", "x.png", bytes) || !bytes.empty() ||
        Eden::UserContent::Entries("/data/synthetic-a05", &folders, &images) || !folders.empty() || !images.empty())
        return 1;
    std::puts("A05_UNQUALIFIED_NATIVE_GATE_OFF_PASS");
}
