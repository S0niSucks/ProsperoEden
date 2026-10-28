// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Eden::Nextendo::Detail {
// Reject ambiguity before the DOM's last-key-wins behavior loses credential/consent fields.
// Keys are decoded by the parser; each object has its own scope, including nested account data.
inline nlohmann::json ParseUniqueJson(std::string_view bytes) {
    using Json = nlohmann::json;
    std::vector<std::set<std::string>> objects;
    bool unique = true;
    auto json = Json::parse(bytes, [&](int, Json::parse_event_t event, Json& value) {
        if (event == Json::parse_event_t::object_start) objects.emplace_back();
        else if (event == Json::parse_event_t::key)
            unique = objects.back().insert(value.get_ref<const std::string&>()).second && unique;
        else if (event == Json::parse_event_t::object_end) objects.pop_back();
        return true;
    });
    return unique ? std::move(json) : Json(Json::value_t::discarded);
}
} // namespace Eden::Nextendo::Detail
