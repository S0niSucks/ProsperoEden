// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// tools/build_identity.py stamps the copied, unstripped ELF before it becomes an eboot.
// Volatile reads prevent LTO from replacing the post-link field with its zero placeholder.
extern "C" const char eden_build_identity[81];
namespace Eden {
inline void ReadBuildIdentity(char (&text)[65]) noexcept {
    const volatile char* field = eden_build_identity + 16;
    for (unsigned n = 0; n < 64; ++n) text[n] = field[n];
    text[64] = '\0';
}
} // namespace Eden
