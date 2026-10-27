// SPDX-License-Identifier: GPL-3.0-or-later
// UI changes are a mailbox, not mutations of the VM's vector while its timing callback runs.
#pragma once
#include <algorithm>
#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "core/memory/dmnt_cheat_types.h"

namespace Eden::LiveCheats {
using Entries = std::vector<Core::Memory::CheatEntry>;
struct Row { size_t index; std::string name; bool enabled; };
struct View { uint64_t generation = 0; std::array<u8, 32> build{}; std::vector<Row> rows; };
inline std::mutex mutex;
inline const void* owner = nullptr;
inline uint64_t generation = 0;
inline std::array<u8, 32> build{};
inline Entries entries;
inline bool pending = false;
inline void Begin(const void* engine, const Entries& list, const std::array<u8, 32>& id) {
    const std::lock_guard lock(mutex);
    owner = engine; ++generation; build = id; entries = list; pending = false;
}
inline void End(const void* engine) {
    const std::lock_guard lock(mutex);
    if (owner != engine) return;
    owner = nullptr; ++generation; entries.clear(); pending = false; build = {};
}
inline View Read() {
    const std::lock_guard lock(mutex);
    View view{generation, build, {}};
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        if (!entry.cheat_id || !entry.definition.num_opcodes) continue; // never toggle master codes
        const auto& name = entry.definition.readable_name;
        view.rows.push_back({i, std::string(name.begin(), std::find(name.begin(), name.end(), '\0')), entry.enabled});
    }
    return view;
}
inline bool Toggle(uint64_t expected_generation, size_t index) {
    const std::lock_guard lock(mutex);
    if (!owner || generation != expected_generation || index >= entries.size() ||
        !entries[index].cheat_id || !entries[index].definition.num_opcodes) return false;
    entries[index].enabled = !entries[index].enabled;
    pending = true;
    return true;
}
inline void Replace(const void* engine, Entries list) {
    const std::lock_guard lock(mutex);
    if (owner != engine) return;
    entries = std::move(list); ++generation; pending = true;
}
inline std::optional<Entries> Take(const void* engine) {
    const std::lock_guard lock(mutex);
    if (owner != engine || !pending) return std::nullopt;
    pending = false;
    return entries;
}
} // namespace Eden::LiveCheats
