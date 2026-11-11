// SPDX-FileCopyrightText: Copyright 2026 Oliv3rmoon
// SPDX-License-Identifier: GPL-3.0-or-later
// Adapted from ProsperoEden dae49cf: explain loader failures and missing key
// generations without logging key material. No filesystem or startup probes.
#pragma once
#include <cstdio>
#include <string>

namespace Eden::LoadFailure {
inline std::string Text(unsigned status, unsigned loader_base, const std::string& loader_message,
                        bool key_problem, int newest_master_key) {
    std::string text = "Loader status " + std::to_string(status);
    if (status >= loader_base && !loader_message.empty()) {
        text += " (" + std::to_string(status - loader_base) + "): " + loader_message;
        if (text.back() != '.') text += '.';
    } else {
        text += '.';
    }
    if (key_problem) {
        text += " The keys file (prod.keys) lacks a key this game needs.";
        if (newest_master_key >= 0) {
            char name[32];
            std::snprintf(name, sizeof(name), "master_key_%02x", newest_master_key);
            text += std::string(" The newest master key in it is ") + name +
                    "; a game made for a newer system version needs keys from that version.";
        } else {
            text += " It holds no master key.";
        }
        text += " See stderr.log.";
    } else {
        text += " Check this ROM, its keys and firmware; see stderr.log.";
    }
    return text;
}
} // namespace Eden::LoadFailure
